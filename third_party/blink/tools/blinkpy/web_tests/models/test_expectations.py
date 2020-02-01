# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""A helper class for reading in and dealing with tests expectations for web tests."""

from collections import defaultdict

import logging
import itertools
import re

from collections import defaultdict

from blinkpy.common import path_finder
from blinkpy.common.memoized import memoized
from blinkpy.web_tests.models.test_configuration import TestConfigurationConverter
from blinkpy.web_tests.models import typ_types

ResultType = typ_types.ResultType

_log = logging.getLogger(__name__)


SPECIAL_PREFIXES = ('# tags:', '# results:', '# conflicts_allowed:')


_PLATFORM_TOKENS_LIST = [
    'Android',
    'Fuchsia',
    'IOS', 'IOS12.2', 'IOS13.0',
    'Linux',
    'Mac', 'Mac10.10', 'Mac10.11', 'Retina', 'Mac10.12', 'Mac10.13', 'Mac10.14',
    'Win', 'Win7', 'Win10'
]


_BUILD_TYPE_TOKEN_LIST = [
    'Release',
    'Debug',
]


EXPECTATION_DESCRIPTIONS = {
    ResultType.Skip: 'skipped',
    ResultType.Pass: 'passes',
    ResultType.Failure: 'failures',
    ResultType.Crash: 'crashes',
    ResultType.Timeout: 'timeouts',
}


class ParseError(Exception):

    def __init__(self, errors):
        self.errors = errors

    def __str__(self):
        return '\n'.join(self.errors)

    def __repr__(self):
        return 'ParseError(errors=%s)' % str(self.errors)


class TestExpectations(object):

    def __init__(self, port, expectations_dict=None):
        self._port = port
        self._system_condition_tags = self._port.get_platform_tags()
        self._expectations = []
        self._expectations_dict = expectations_dict or port.expectations_dict()
        filesystem = self._port.host.filesystem
        expectation_errors = []

        for path, content in self._expectations_dict.items():
            test_expectations = typ_types.TestExpectations(tags=self._system_condition_tags)
            ret, errors = test_expectations.parse_tagged_list(
                content, tags_conflict=self._tags_conflict)
            if ret:
                expectation_errors.append('Parsing file %s produced following errors\n%s' % (path, errors))
            self._expectations.append(test_expectations)

        if port.get_option('ignore_tests', []):
            content = '# results: [ Skip ]\n'
            for pattern in set(port.get_option('ignore_tests', [])):
                if filesystem.isdir(filesystem.join(self._port.web_tests_dir(), pattern)):
                    pattern += '*'
                content += '%s [ Skip ]\n' % pattern
            test_expectations = typ_types.TestExpectations()
            ret, errors = test_expectations.parse_tagged_list(content)
            if ret:
                expectation_errors.append('Parsing patterns passed through --ignore produced the following errors\n%s' %
                                          errors)
            self._expectations.append(test_expectations)
        if expectation_errors:
            raise ParseError(expectation_errors)
        self._add_expectations_from_bot()

    @property
    def expectations(self):
        return self._expectations[:]

    @property
    def general_expectations(self):
        # Get typ.TestExpectations instance for general
        # test expectations file
        return self._expectations[0]

    @property
    def port(self):
        return self._port

    @property
    def expectations_dict(self):
        return self._expectations_dict

    @memoized
    def _os_to_version(self):
        os_to_version = {}
        for os, os_versions in self._port.configuration_specifier_macros().items():
            for version in os_versions:
                os_to_version[version.lower()] = os.lower()
        return os_to_version

    def _tags_conflict(self, t1, t2):
        os_to_version = self._os_to_version()
        if not t1 in os_to_version and not t2 in os_to_version:
            return t1 != t2
        elif t1 in os_to_version and t2 in os_to_version:
            return t1 != t2
        elif t1 in os_to_version:
            return os_to_version[t1] != t2
        else:
            return os_to_version[t2] != t1

    def merge_raw_expectations(self, content):
        test_expectations = typ_types.TestExpectations()
        test_expectations.parse_tagged_list(content)
        self._expectations.append(test_expectations)

    def get_expectations(self, test):
        results = set()
        reasons = set()
        is_slow_test = False
        for test_exp in self._expectations:
            expected_results = test_exp.expectations_for(test)
            # The return Expectation instance from expectations_for has the default
            # PASS expected result. If there are no expected results in the first
            # file and there are expected results in the second file, then the JSON
            # results will show an expected per test field with PASS and whatever the
            # expected results in the second file are.
            if not (len(expected_results.results) == 1 and
                    ResultType.Pass in expected_results.results):
                results.update(expected_results.results)
            is_slow_test |= expected_results.is_slow_test
            reasons.update(expected_results.reason.split())
        # If the results set is empty then the Expectation constructor
        # will set the expected result to Pass.
        return typ_types.Expectation(
            test=test, results=results, is_slow_test=is_slow_test,
            reason=' '.join(reasons))

    def get_tests_with_expected_result(self, result):
        """This method will return a list of tests and directories which
        have the result argument value in its expected results

        Args:
          result: ResultType value, i.e ResultType.Skip"""
        tests = []
        for test_exp in self.expectations:
            tests.extend([test_name for test_name in test_exp.individual_exps.keys()])
            tests.extend([dir_name[:-1] for dir_name in test_exp.glob_exps.keys()
                          if self.port.test_isdir(dir_name[:-1])])
        return {test_name for test_name in tests
                if result in self.get_expectations(test_name).results}

    def matches_an_expected_result(self, test, result):
        expected_results = self.get_expectations(test).results
        return result in expected_results

    def _add_expectations_from_bot(self):
        # FIXME: With mode 'very-flaky' and 'maybe-flaky', this will show the expectations entry in the flakiness
        # dashboard rows for each test to be whatever the bot thinks they should be. Is this a good thing?
        bot_expectations = self._port.bot_expectations()
        raw_expectations = '# results: [ Failure Pass Crash Skip Timeout ]\n'
        for test, results in bot_expectations.items():
            raw_expectations += typ_types.Expectation(test=test, results=results).to_string() + '\n'
        self.merge_raw_expectations(raw_expectations)


class SystemConfigurationRemover(object):
    """This class can be used to remove system version configurations (i.e Mac10.10 or trusty)
    from a test expectation. It will also split an expectation with no OS or OS version specifiers
    into expectations for OS versions that were not removed, and consolidate expectations for all
    versions of an OS into an expectation with the OS specifier.
    """

    def __init__(self, test_expectations):
        self._test_expectations = test_expectations
        self._configuration_specifiers_dict = {}
        for os, os_versions in  (
              self._test_expectations.port.configuration_specifier_macros().items()):
            self._configuration_specifiers_dict[os.lower()] = (
                frozenset(version.lower() for version in os_versions))
        self._os_specifiers = frozenset(os for os in self._configuration_specifiers_dict.keys())
        self._version_specifiers = frozenset(
            specifier.lower() for specifier in
            reduce(lambda x,y: x|y, self._configuration_specifiers_dict.values()))
        self._deleted_lines = set()

    def _split_configuration(self, exp, versions_to_remove):
        build_specifiers = set()
        os_specifiers = ({os for os in self._os_specifiers if
                          versions_to_remove & self._configuration_specifiers_dict[os]} & exp.tags)
        if os_specifiers:
            # If an expectations tag list has an OS tag which has several versions which are
            # in the versions_to_remove set, create expectations for versions that are not in
            # the versions_to_remove set which fall under the OS specifier.
            build_specifiers = exp.tags - os_specifiers
            os_specifier = os_specifiers.pop()
            system_specifiers = (
                set(version for version in self._configuration_specifiers_dict[os_specifier]) -
                versions_to_remove)
        elif self._os_specifiers & exp.tags:
            # If there is an OS tag in the expectation's tag list which does not have
            # versions in the versions_to_remove list then return the expectation.
            return [exp]
        else:
            # If there are no OS tags in the expectation's tag list, then create an
            # expectation for each version that is not in the versions_to_remove list
            system_specifiers = set(self._version_specifiers - versions_to_remove)
            for os, os_versions in self._configuration_specifiers_dict.items():
                # If all the versions of an OS are in the system specifiers set, then
                # replace all those specifiers with the OS specifier.
                if os_versions.issubset(system_specifiers):
                    for version in os_versions:
                        system_specifiers.remove(version)
                    system_specifiers.add(os)
        return [
            typ_types.Expectation(
                tags=set([specifier])|build_specifiers, results=exp.results, is_slow_test=exp.is_slow_test,
                reason=exp.reason, test=exp.test, lineno=exp.lineno, trailing_comments=exp.trailing_comments)
            for specifier in sorted(system_specifiers)]

    def remove_os_versions(self, test_name, versions_to_remove):
        test_to_exps = self._test_expectations.general_expectations.individual_exps
        versions_to_remove = frozenset(specifier.lower() for specifier in versions_to_remove)
        if test_name not in test_to_exps:
            return
        if not versions_to_remove:
            # This will prevent making changes to test expectations which have no OS versions to remove.
            return
        exps = test_to_exps[test_name]
        delete_exps = []
        add_exps = []
        for exp in exps:
            if exp.tags & versions_to_remove:
                # if an expectation's tag set has versions that are being removed then add the
                # exp to the delete_exps list.
                delete_exps.append(exp)
            elif not exp.tags & self._version_specifiers:
                # if it does not have versions that need to be deleted then attempt to split
                # the expectation
                delete_exps.append(exp)
                add_exps.extend(self._split_configuration(exp, versions_to_remove))
        for exp in delete_exps:
            # delete expectations in delete_exps
            self._deleted_lines.add(exp.lineno)
            exps.remove(exp)
        # add new expectations which were derived when an expectation was split
        exps.extend(add_exps)
        if not exps:
            test_to_exps.pop(test_name)
        return test_to_exps

    def update_expectations(self):
        path, raw_content = self._test_expectations.expectations_dict.items()[0]
        lines = raw_content.split('\n')
        new_lines = []
        lineno_to_exps = defaultdict(list)
        for exps in self._test_expectations.general_expectations.individual_exps.values():
            for e in exps:
                # map expectations to their line number in the expectations file
                lineno_to_exps[e.lineno].append(e)
        for lineno, line in enumerate(lines, 1):
            if lineno not in lineno_to_exps and lineno not in self._deleted_lines:
                # add empty lines and comments to the new_lines list
                new_lines.append(line)
            elif lineno in lineno_to_exps:
                # if an expectation was not deleted then convert the expectation
                # to a string and add it to the new_lines list.
                for exp in lineno_to_exps[lineno]:
                    # We need to escape the asterisk in test names.
                    new_lines.append(exp.to_string())
            elif lineno in self._deleted_lines and lineno + 1 not in lineno_to_exps:
                # If the expectation was deleted and it is the last one in a
                # block of expectations then delete all the white space and
                # comments associated with it.
                while (new_lines and new_lines[-1].strip().startswith('#') and
                       not any(new_lines[-1].strip().startswith(prefix) for prefix in SPECIAL_PREFIXES)):
                    new_lines.pop(-1)
                while new_lines and not new_lines[-1].strip():
                    new_lines.pop(-1)
        new_content = '\n'.join(new_lines)
        self._test_expectations.port.host.filesystem.write_text_file(path, new_content)
        return new_content

