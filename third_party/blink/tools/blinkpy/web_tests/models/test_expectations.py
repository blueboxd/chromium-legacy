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

import logging
import itertools
import re

from collections import defaultdict
from collections import OrderedDict

from blinkpy.common import path_finder
from blinkpy.common.memoized import memoized
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


class _NotExpectation(typ_types.Expectation):
    '''This class is a placeholder for emtpy lines or comments in the
    test expectations file. It has the same API as typ_types.Expectations.
    However the test member variable is set to an empty string and there
    are no expected results in this line.'''

    def __init__(self, line, lineno):
        super(_NotExpectation, self).__init__(test='', lineno=lineno)
        self._line = line

    def to_string(self):
        return self._line


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

        # Separate expectations from flag specific files into a base expectations list and
        # flag expectations list. We will also store the blink flags in a list.
        self._flags = []
        self._flag_expectations = []
        self._base_expectations = []
        # Map each expectations file to a list of Expectation instances representing each line.
        self._file_to_lines = defaultdict(list)
        # Map each file to flags which represent if its list of Expectation instances were
        # modified
        self._was_modified = {}
        # Map each file to flags which represent if the changes made to the file's lines were
        # written to the file.
        self._file_changes_committed = {}

        for path, content in self._expectations_dict.items():
            test_expectations = typ_types.TestExpectations(tags=self._system_condition_tags)
            ret, errors = test_expectations.parse_tagged_list(content, tags_conflict=self._tags_conflict)
            if ret:
                expectation_errors.append('Parsing file %s produced following errors\n%s' % (path, errors))
            self._expectations.append(test_expectations)
            flag_match = re.match('.*' + port.FLAG_EXPECTATIONS_PREFIX + '(.*)', path)

            self._reset_lines(path)
            self._file_changes_committed[path] = False

            # If file is a flag specific file, store the typ.TestExpectation
            # instance in _flag_expectations list, otherwise store it in _base_expectations
            if flag_match:
                self._flags.append(flag_match.group(1))
                self._flag_expectations.append(test_expectations)
            else:
                self._base_expectations.append(test_expectations)

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

    def get_updated_lines(self, path):
        """This method returns the Expectation instances for each line
        in an expectations file. If there were any modifications made
        through the remove_expectations or add_expectations member methods
        then this method will call the _reset_lines method in order to update
        the list of lines for the expectations file.

        args:
            path: Absolute path of expectations file."""
        if self._was_modified[path]:
            self._reset_lines(path)
        return self._file_to_lines[path]

    @staticmethod
    def _maybe_remove_comments_and_whitespace(lines):
        """If the last expectation in a block is deleted, then remove all associated
        comments and white spaces.

        args:
            lines: Array which contains Expectation instances for each line in an
                   expectations file."""
        # remove comments associated with deleted expectation
        while (lines and lines[-1].to_string().strip().startswith('#') and
               not any(lines[-1].to_string().strip().startswith(prefix) for prefix in SPECIAL_PREFIXES)):
            lines.pop()

        # remove spaces above expectation
        while lines and lines[-1].to_string().strip() == '':
            lines.pop()

    def _reset_lines(self, path):
        """This method resets Expectation instances for each line in
        a file whenever they are requested via the get_updated_lines member method
        and there were modifications made to the lines.

        args:
            path: Absolute path of the expectations file."""
        baseline_linenos = {e.lineno for e in self._file_to_lines[path] if not isinstance(e, _NotExpectation)}
        not_expectation_linenos = {e.lineno: e for e in self._file_to_lines[path] if e.lineno not in baseline_linenos}
        content = self._expectations_dict[path]
        idx = self._expectations_dict.keys().index(path)
        typ_expectations = self._expectations[idx]
        lines = []

        # Store Expectation instances for each line
        lineno_to_exps = defaultdict(list)

        for pattern_to_exps in (typ_expectations.individual_exps, typ_expectations.glob_exps):
            for exps in pattern_to_exps.values():
                for exp in exps:
                    lineno_to_exps[exp.lineno].append(exp)

        removed_linenos = baseline_linenos - set(lineno_to_exps.keys())

        for lineno, line in enumerate(content.splitlines(), 1):
            if lineno not in lineno_to_exps and lineno not in removed_linenos:
                lines.append(_NotExpectation(line, lineno))
            elif lineno in removed_linenos:
                if lineno + 1 not in lineno_to_exps:
                    self._maybe_remove_comments_and_whitespace(lines)
            else:
                if lineno in not_expectation_linenos:
                    lines.append(not_expectation_linenos[lineno])
                lines.extend(lineno_to_exps[lineno])
                lineno_to_exps.pop(lineno)

            # set the lineno of each Expectation or _NotExpectation instance
            if lines:
                lines[-1].lineno = len(lines)

        # Add new expectations that were not part of the file before
        if lineno_to_exps:
            lines.append(_NotExpectation('', len(lines) + 1))
        for lineno, extras in lineno_to_exps.items():
            for line in extras:
                lines.append(line)
                lines[-1].lineno = len(lines)

        self._file_to_lines[path] = lines
        self._was_modified[path] = False

    def commit_changes(self):
        """Writes to the expectations files any modifications made
        through the remove_expectations or add_expectations member
        methods"""
        for path, commited in self._file_changes_committed.items():
            if commited:
                continue
            new_content = '\n'.join([e.to_string() for e in self.get_updated_lines(path)]) + '\n'
            self._expectations_dict[path] = new_content
            self._port.host.filesystem.write_text_file(path, new_content)
            self._file_changes_committed[path] = True

    @property
    def expectations(self):
        return self._expectations[:]

    @property
    def flag_name(self):
        return ' '.join(self._flags)

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

    @staticmethod
    def _get_expectations(expectations, test):
        results = set()
        reasons = set()
        is_slow_test = False
        trailing_comments = ''
        for test_exp in expectations:
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
            # Typ will leave a newline at the end of trailing_comments, so we
            # can just concatenate here and still have comments from different
            # files be separated by newlines.
            trailing_comments += expected_results.trailing_comments
        # If the results set is empty then the Expectation constructor
        # will set the expected result to Pass.
        return typ_types.Expectation(
            test=test, results=results, is_slow_test=is_slow_test, reason=' '.join(reasons), trailing_comments=trailing_comments)

    def get_expectations(self, test):
        return self._get_expectations(self._expectations, test)

    def get_flag_expectations(self, test):
        exp = self._get_expectations(self._flag_expectations, test)
        if exp.is_slow_test or exp.results != set([ResultType.Pass]):
            return exp
        return None

    def get_base_expectations(self, test):
        return self._get_expectations(self._base_expectations, test)

    def get_tests_with_expected_result(self, result):
        """This method will return a list of tests and directories which
        have the result argument value in its expected results

        args:
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

    def remove_expectations(self, path, exps):
        """This method removes Expectation instances from an expectations file.
        It will delete the line in the expectations file associated with the
        Expectation instance.

        args:
            path: Absolute path of file where the Expectation instances
                  came from.
            exps: List of Expectation instances to be deleted."""
        idx = self._expectations_dict.keys().index(path)
        typ_expectations = self._expectations[idx]

        for exp in exps:
            if exp.is_glob:
                pattern_to_exps = typ_expectations.glob_exps
            else:
                pattern_to_exps = typ_expectations.individual_exps
            pattern_to_exps[exp.test].remove(exp)
            if not pattern_to_exps[exp.test]:
                pattern_to_exps.pop(exp.test)

        self._was_modified[path] = True
        self._file_changes_committed[path] = False

    def add_expectations(self, path, exps, lineno=0):
        """This method adds Expectation instances to an expectations file. It will
        add the new instances after the line number passed through the lineno parameter.
        If the lineno is set to a value outside the range of line numbers in the file
        then it will append the expectations to the end of the file

        args:
            path: Absolute path of file where expectations will be added to.
            exps: List of Expectation instances to be added to the file.
            lineno: Line number in expectations file where the expectations will
                    be added."""
        idx = self._expectations_dict.keys().index(path)
        typ_expectations = self._expectations[idx]
        added_glob = False

        for exp in exps:
            exp.lineno = lineno

        for exp in exps:
            added_glob |= exp.is_glob
            if exp.is_glob:
                typ_expectations.glob_exps.setdefault(exp.test, []).append(exp)
            else:
                typ_expectations.individual_exps.setdefault(exp.test,[]).append(exp)

        if added_glob:
            glob_exps = reduce(lambda x,y: x + y, typ_expectations.glob_exps.values())
            glob_exps.sort(key=lambda e: len(e.test), reverse=True)
            typ_expectations.glob_exps = OrderedDict()
            for exp in glob_exps:
                typ_expectations.glob_exps.setdefault(exp.test, []).append(exp)

        self._was_modified[path] = True
        self._file_changes_committed[path] = False


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
        self._generic_exp_file_path = self._test_expectations.port.path_to_generic_test_expectations_file()

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
        versions_to_remove = frozenset(specifier.lower() for specifier in versions_to_remove)
        if not versions_to_remove:
            # This will prevent making changes to test expectations which have no OS versions to remove.
            return

        exp_lines = self._test_expectations.get_updated_lines(self._generic_exp_file_path)
        delete_exps = []

        for exp in exp_lines:
            if exp.test != test_name:
                continue
            if exp.tags & versions_to_remove:
                self._test_expectations.remove_expectations(self._generic_exp_file_path, [exp])
            elif not exp.tags & self._version_specifiers:
                self._test_expectations.add_expectations(
                    self._generic_exp_file_path, self._split_configuration(exp, versions_to_remove), exp.lineno)
                self._test_expectations.remove_expectations(self._generic_exp_file_path, [exp])

    def update_expectations(self):
        self._test_expectations.commit_changes()

