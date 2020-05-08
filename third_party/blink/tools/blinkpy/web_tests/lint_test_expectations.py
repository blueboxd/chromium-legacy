# Copyright (C) 2012 Google Inc. All rights reserved.
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

import json
import logging
import optparse
import traceback

from blinkpy.common import exit_codes
from blinkpy.common.host import Host
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.web_tests.models.test_expectations import (TestExpectations,
                                                        ParseError)

from blinkpy.web_tests.port.factory import platform_options

_log = logging.getLogger(__name__)


def PresubmitCheckTestExpectations(input_api, output_api):
    os_path = input_api.os_path
    lint_path = os_path.join(
        os_path.dirname(os_path.abspath(__file__)), '..', '..',
        'lint_test_expectations.py')
    _, errs = input_api.subprocess.Popen(
        [input_api.python_executable, lint_path],
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE).communicate()
    if not errs:
        return [
            output_api.PresubmitError("lint_test_expectations.py failed "
                                      "to produce output; check by hand. ")
        ]
    if errs.strip() != 'Lint succeeded.':
        return [output_api.PresubmitError(errs)]
    return []


def lint(host, options):
    port = host.port_factory.get(options.platform)

    # Add all extra expectation files to be linted.
    options.additional_expectations.extend([
        host.filesystem.join(port.web_tests_dir(), 'android',
                             'ClankWPTOverrideExpectations'),
        host.filesystem.join(port.web_tests_dir(), 'android',
                             'WebviewWPTOverrideExpectations'),
        host.filesystem.join(port.web_tests_dir(), 'android',
                             'WeblayerWPTOverrideExpectations'),
        host.filesystem.join(port.web_tests_dir(), 'android',
                             'AndroidWPTNeverFixTests'),
        host.filesystem.join(port.web_tests_dir(), 'WPTOverrideExpectations'),
        host.filesystem.join(port.web_tests_dir(), 'WebGPUExpectations'),
    ])

    ports_to_lint = [
        host.port_factory.get(name, options=options)
        for name in host.port_factory.all_port_names(options.platform)
    ]

    # In general, the set of TestExpectation files should be the same for
    # all ports. However, the method used to list expectations files is
    # in Port, and the TestExpectations constructor takes a Port.
    # Perhaps this function could be changed to just use one Port
    # (the default Port for this host) and it would work the same.

    failures = []
    expectations_dict = {}
    all_system_specifiers = set()
    all_build_specifiers = set(ports_to_lint[0].ALL_BUILD_TYPES)

    # TODO(crbug.com/986447) Remove the checks below after migrating the expectations
    # parsing to Typ. All the checks below can be handled by Typ.

    for port in ports_to_lint:
        expectations_dict.update(port.all_expectations_dict())
        config_macro_dict = port.configuration_specifier_macros()
        if config_macro_dict:
            all_system_specifiers.update(
                {s.lower()
                 for s in config_macro_dict.keys()})
            all_system_specifiers.update({
                s.lower()
                for s in reduce(lambda x, y: x + y, config_macro_dict.values())
            })
        for path in port.extra_expectations_files():
            if host.filesystem.exists(path):
                expectations_dict[path] = host.filesystem.read_text_file(path)

    for path, content in expectations_dict.items():
        # Check the expectations file content
        failures.extend(_check_expectations_file_content(content))

        # Create a TestExpectations instance and see if an exception is raised
        try:
            test_expectations = TestExpectations(
                ports_to_lint[0], expectations_dict={path: content})
            # Check each expectation for issues
            failures.extend(
                _check_expectations(host, ports_to_lint[0], path,
                                    test_expectations))
        except ParseError as error:
            _log.error(str(error))
            failures.append(str(error))
            _log.error('')

    return failures


def _check_expectations_file_content(content):
    failures = []
    for lineno, line in enumerate(content.splitlines(), 1):
        if not line.strip() or line.strip().startswith('#'):
            continue
        # check for test expectations that start with leading spaces
        if line.startswith(' '):
            error = (('%s:%d Line %d has a test expectation'
                      ' that has leading spaces.') %
                     (host.filesystem.basename(path), lineno, lineno))
            _log.error(error)
            failures.append(error)
            _log.error('')

        # check for test expectations that have a Bug(...) as the reason
        if line.startswith('Bug('):
            error = (
                ("%s:%d Expectation '%s' has the Bug(...) token, "
                 "The token has been removed in the new expectations format") %
                (host.filesystem.basename(path), lineno, line))
            _log.error(error)
            failures.append(error)
            _log.error('')

    return failures


def _check_existence(host, port, path, expectations):
    failures = []
    for exp in expectations:
        if not exp.test:
            continue
        test = exp.test
        if exp.is_glob:
            # This is ensured in typ.Expectation.
            assert test.endswith('*')
            test = test[:-1]
        if not port.test_exists(test):
            error = "{}:{} Test does not exist: {}".format(
                host.filesystem.basename(path), exp.lineno, exp.test)
            _log.error(error)
            failures.append(error)
    return failures


def _check_directory_glob(host, port, path, expectations):
    failures = []
    for exp in expectations:
        if not exp.test or exp.is_glob:
            continue

        test_name, _ = port.split_webdriver_test_name(exp.test)
        index = test_name.find('?')
        if index != -1:
            test_name = test_name[:index]

        if port.test_isdir(test_name):
            error = (
                ("%s:%d Expectation '%s' is for a directory, however "
                 "the name in the expectation does not have a glob in the end")
                % (host.filesystem.basename(path), exp.lineno, test_name))
            _log.error(error)
            failures.append(error)
            _log.error('')

    return failures


def _check_redundant_virtual_expectations(host, port, path, expectations):
    # FlagExpectations are not checked because something like the following in
    # a flag-specific expectations file looks redundant but is needed to
    # override the virtual expectations in TestExpectations. For example, in
    # the main TestExpectations:
    #   foo/bar/test.html [ Failure ]
    #   virtual/suite/foo/bar/test.html [ Timeout ]
    # and in a flag expectation file, we want to override both to [ Crash ]:
    #   foo/bar/test.html [ Crash ]
    #   virtual/suite/foo/bar/test.html [ Crash ]
    if 'FlagExpectations' in path:
        return []

    failures = []
    expectations_by_test = {}
    for exp in expectations:
        if exp.test:
            expectations_by_test.setdefault(exp.test, []).append(exp)

    for exp in expectations:
        if not exp.test:
            continue

        base_test = port.lookup_virtual_test_base(exp.test)
        if not base_test:
            continue

        for base_exp in expectations_by_test.get(base_test, []):
            if (base_exp.results == exp.results
                    and base_exp.is_slow_test == exp.is_slow_test
                    and base_exp.tags.issubset(exp.tags)):
                error = "{}:{} Expectation '{}' is redundant with '{}' in line {}".format(
                    host.filesystem.basename(path), exp.lineno, exp.test,
                    base_test, base_exp.lineno)
                _log.error(error)
                failures.append(error)

    return failures


def _check_expectations(host, port, path, test_expectations):
    # Check for original expectation lines (from get_updated_lines) instead of
    # expectations filtered for the current port (test_expectations).
    expectations = test_expectations.get_updated_lines(path)
    failures = _check_existence(host, port, path, expectations)
    failures.extend(_check_directory_glob(host, port, path, expectations))
    failures.extend(
        _check_redundant_virtual_expectations(host, port, path, expectations))
    return failures


def check_virtual_test_suites(host, options):
    port = host.port_factory.get(options=options)
    fs = host.filesystem
    web_tests_dir = port.web_tests_dir()
    virtual_suites = port.virtual_test_suites()
    virtual_suites.sort(key=lambda s: s.full_prefix)

    failures = []
    for suite in virtual_suites:
        suite_comps = suite.full_prefix.split(port.TEST_PATH_SEPARATOR)
        prefix = suite_comps[1]
        normalized_bases = [port.normalize_test_name(b) for b in suite.bases]
        normalized_bases.sort()
        for i in range(1, len(normalized_bases)):
            for j in range(0, i):
                if normalized_bases[i].startswith(normalized_bases[j]):
                    failure = 'Base "{}" starts with "{}" in the same virtual suite "{}", so is redundant.'.format(
                        normalized_bases[i], normalized_bases[j], prefix)
                    _log.error(failure)
                    failures.append(failure)

        # A virtual test suite needs either
        # - a top-level README.md (e.g. virtual/foo/README.md)
        # - a README.txt for each covered directory (e.g.
        #   virtual/foo/http/tests/README.txt, virtual/foo/fast/README.txt, ...)
        comps = [web_tests_dir] + suite_comps + ['README.md']
        path_to_readme_md = fs.join(*comps)
        for base in suite.bases:
            if not base:
                failure = 'Base value in virtual suite "{}" should not be an empty string'.format(
                    prefix)
                _log.error(failure)
                failures.append(failure)
                continue
            base_comps = base.split(port.TEST_PATH_SEPARATOR)
            absolute_base = port.abspath_for_test(base)
            if fs.isfile(absolute_base):
                del base_comps[-1]
            elif not fs.isdir(absolute_base):
                failure = 'Base "{}" in virtual suite "{}" must refer to a real file or directory'.format(
                    base, prefix)
                _log.error(failure)
                failures.append(failure)
                continue
            comps = [web_tests_dir] + suite_comps + base_comps + ['README.txt']
            path_to_readme_txt = fs.join(*comps)
            if (not fs.exists(path_to_readme_md)
                    and not fs.exists(path_to_readme_txt)):
                failure = '"{}" and "{}" are both missing (each virtual suite must have one).'.format(
                    path_to_readme_txt, path_to_readme_md)
                _log.error(failure)
                failures.append(failure)

    if failures:
        _log.error('')
    return failures


def check_smoke_tests(host, options):
    port = host.port_factory.get(options=options)
    smoke_tests_file = host.filesystem.join(port.web_tests_dir(), 'SmokeTests')
    failures = []
    if not host.filesystem.exists(smoke_tests_file):
        return failures

    smoke_tests = host.filesystem.read_text_file(smoke_tests_file)
    line_number = 0
    parsed_lines = {}
    for line in smoke_tests.split('\n'):
        line_number += 1
        line = line.split('#')[0].strip()
        if not line:
            continue
        failure = ''
        if line in parsed_lines:
            failure = '%s:%d duplicate with line %d: %s' % \
                (smoke_tests_file, line_number, parsed_lines[line], line)
        elif not port.test_exists(line):
            failure = '%s:%d Test does not exist: %s' % (smoke_tests_file,
                                                         line_number, line)
        if failure:
            _log.error(failure)
            failures.append(failure)
        parsed_lines[line] = line_number
    if failures:
        _log.error('')
    return failures


def run_checks(host, options):
    failures = []
    failures.extend(lint(host, options))
    failures.extend(check_virtual_test_suites(host, options))
    failures.extend(check_smoke_tests(host, options))

    if options.json:
        with open(options.json, 'w') as f:
            json.dump(failures, f)

    if failures:
        _log.error('Lint failed.')
        return 1
    else:
        _log.info('Lint succeeded.')
        return 0


def main(argv, stderr, host=None):
    parser = optparse.OptionParser(
        option_list=platform_options(use_globs=True))
    parser.add_option('--json', help='Path to JSON output file')
    parser.add_option(
        '--verbose',
        action='store_true',
        default=False,
        help='log extra details that may be helpful when debugging')
    parser.add_option(
        '--additional-expectations',
        action='append',
        default=[],
        help='paths to additional expectation files to lint.')

    options, _ = parser.parse_args(argv)

    if not host:
        if options.platform and 'test' in options.platform:
            # It's a bit lame to import mocks into real code, but this allows the user
            # to run tests against the test platform interactively, which is useful for
            # debugging test failures.
            from blinkpy.common.host_mock import MockHost
            host = MockHost()
        else:
            host = Host()

    if options.verbose:
        configure_logging(logging_level=logging.DEBUG, stream=stderr)
        # Print full stdout/stderr when a command fails.
        host.executive.error_output_limit = None
    else:
        # PRESUBMIT.py relies on our output, so don't include timestamps.
        configure_logging(
            logging_level=logging.INFO, stream=stderr, include_time=False)

    try:
        exit_status = run_checks(host, options)
    except KeyboardInterrupt:
        exit_status = exit_codes.INTERRUPTED_EXIT_STATUS
    except Exception as error:  # pylint: disable=broad-except
        print >> stderr, '\n%s raised: %s' % (error.__class__.__name__, error)
        traceback.print_exc(file=stderr)
        exit_status = exit_codes.EXCEPTIONAL_EXIT_STATUS

    return exit_status
