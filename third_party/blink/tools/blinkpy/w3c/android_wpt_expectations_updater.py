# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates expectations for Android WPT bots.

Specifically, this class fetches results from try bots for the current CL, then
(1) updates browser specific expectation files for Androids browsers like Weblayer.

We needed an Android flavour of the WPTExpectationsUpdater class because
(1) Android bots don't currently produce output that can be baselined, therefore
    we only update expectations in the class below.
(2) For Android we write test expectations to browser specific expectation files,
    not the regular web test's TestExpectation and NeverFixTests file.
(3) WPTExpectationsUpdater can only update expectations for the blink_web_tests
    and webdriver_test_suite steps. Android bots may run several WPT steps, so
    the script needs to be able to update expectations for multiple steps.
"""

import logging
from collections import defaultdict, namedtuple

from blinkpy.common.host import Host
from blinkpy.common.memoized import memoized
from blinkpy.common.system.executive import ScriptError
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.models.typ_types import Expectation
from blinkpy.web_tests.port.android import (
    PRODUCTS, PRODUCTS_TO_STEPNAMES, PRODUCTS_TO_BROWSER_TAGS,
    PRODUCTS_TO_EXPECTATION_FILE_PATHS)

_log = logging.getLogger(__name__)

AndroidConfig = namedtuple('AndroidConfig', ['port_name', 'browser'])


class AndroidWPTExpectationsUpdater(WPTExpectationsUpdater):
    MARKER_COMMENT = '# Add untriaged failures in this block'
    UMBRELLA_BUG = 'crbug.com/1050754'

    def __init__(self, host, args=None):
        super(AndroidWPTExpectationsUpdater, self).__init__(host, args)
        expectations_dict = {}
        for product in self.options.android_product:
            path = PRODUCTS_TO_EXPECTATION_FILE_PATHS[product]
            expectations_dict.update(
                {path: self.host.filesystem.read_text_file(path)})

        self._test_expectations = TestExpectations(
            self.port, expectations_dict=expectations_dict)

    def _get_web_test_results(self, build):
        """Gets web tests results for Android builders. We need to
        bypass the step which gets the step names for the builder
        since it does not currently work for Android.

        Args:
            build: Named tuple containing builder name and number

        Returns:
            returns: List of web tests results for each web test step
            in build.
        """
        results_sets = []
        build_specifiers = self._get_build_specifiers(build)
        for product in self.options.android_product:
            if product in build_specifiers:
                step_name = PRODUCTS_TO_STEPNAMES[product]
                results_sets.append(self.host.results_fetcher.fetch_results(
                    build, True, '%s (with patch)' % step_name))
        return filter(None, results_sets)

    def get_builder_configs(self, build, results_set=None):
        """Gets step name from WebTestResults instance and uses
        that to create AndroidConfig instances. It also only
        returns valid configs for the android products passed
        through the --android-product command line argument.

        Args:
            build: Build object that contains the builder name and
                build number.
            results_set: WebTestResults instance. If this variable
                then it will return a list of android configs for
                each product passed through the --android-product
                command line argument.

        Returns:
            List of valid android configs for products passed through
            the --android-product command line argument."""
        configs = []
        if not results_set:
            build_specifiers = self._get_build_specifiers(build)
            products = build_specifiers & {
                s.lower() for s in self.options.android_product}
        else:
            step_name = results_set.step_name()
            step_name = step_name[: step_name.index(' (with patch)')]
            product = {s: p for p, s in PRODUCTS_TO_STEPNAMES.items()}[step_name]
            products = {product}

        for product in products:
            browser = PRODUCTS_TO_BROWSER_TAGS[product]
            configs.append(
                AndroidConfig(port_name=self.port_name(build), browser=browser))
        return configs

    @memoized
    def _get_build_specifiers(self, build):
        return {s.lower() for s in
                self.host.builders.specifiers_for_builder(build.builder_name)}

    def can_rebaseline(self, *_):
        """Return False since we cannot rebaseline tests for
        Android at the moment."""
        return False

    @memoized
    def _get_marker_line_number(self, path):
        for line in self._test_expectations.get_updated_lines(path):
            if line.to_string() == self.MARKER_COMMENT:
                return line.lineno
        raise ScriptError('Marker comment does not exist in %s' % path)

    def write_to_test_expectations(self, test_to_results):
        """Each expectations file is browser specific, and currently only
        runs on pie. Therefore we do not need any configuration specifiers
        to anotate expectations for certain builds.

        Args:
            test_to_results: A dictionary that maps test names to another
            dictionary which maps a tuple of build configurations and to
            a test result.
        Returns:
            Dictionary mapping test names to lists of expectation strings.
        """
        browser_to_exp_path = {
            browser: PRODUCTS_TO_EXPECTATION_FILE_PATHS[product]
            for product, browser in PRODUCTS_TO_BROWSER_TAGS.items()}
        untriaged_exps = defaultdict(dict)

        for path in self._test_expectations.expectations_dict:
            marker_lineno = self._get_marker_line_number(path)
            exp_lines = self._test_expectations.get_updated_lines(path)
            for i in range(marker_lineno, len(exp_lines)):
                if (not exp_lines[i].to_string().strip() or
                        exp_lines[i].to_string().startswith('#')):
                    break
                untriaged_exps[path][exp_lines[i].test] = exp_lines[i]

        for path, test_exps in untriaged_exps.items():
            self._test_expectations.remove_expectations(
                path, test_exps.values())

        for results_test_name, platform_results in test_to_results.items():
            exps_test_name = 'external/wpt/%s' % results_test_name
            for configs, test_results in platform_results.items():
                for config in configs:
                    path = browser_to_exp_path[config.browser]
                    # no system specifiers are necessary because we are
                    # writing to browser specific expectations files for
                    # only one Android version.
                    unexpected_results = {r for r in test_results.actual.split()
                                          if r not in test_results.expected.split()}

                    if exps_test_name not in untriaged_exps[path]:
                        untriaged_exps[path][exps_test_name] = Expectation(
                            test=exps_test_name, reason=self.UMBRELLA_BUG,
                            results=unexpected_results)
                    else:
                        untriaged_exps[path][exps_test_name].add_expectations(
                            unexpected_results, reason=self.UMBRELLA_BUG)

        for path in untriaged_exps:
            marker_lineno = self._get_marker_line_number(path)
            self._test_expectations.add_expectations(
                path,
                sorted(untriaged_exps[path].values(), key=lambda e: e.test),
                marker_lineno)

        self._test_expectations.commit_changes()
        # TODO(rmhasan): Return dictionary mapping test names to lists of
        # test expectation strings.
        return {}

    def _is_wpt_test(self, _):
        """On Android we use the wpt executable. The test results do not include
        the external/wpt prefix. Therefore we need to override this function for
        Android and return True for all tests since we only run WPT on Android.
        """
        return True

    @memoized
    def _get_try_bots(self):
        return self.host.builders.filter_builders(
            is_try=True, include_specifiers=self.options.android_product)
