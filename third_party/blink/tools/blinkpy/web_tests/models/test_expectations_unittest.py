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

from collections import OrderedDict
import optparse
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.output_capture import OutputCapture
from blinkpy.web_tests.models.test_configuration import TestConfiguration, TestConfigurationConverter
from blinkpy.web_tests.models.test_expectations import TestExpectations, SystemConfigurationRemover, ParseError
from blinkpy.web_tests.models.typ_types import ResultType


class Base(unittest.TestCase):
    # Note that all of these tests are written assuming the configuration
    # being tested is Windows 7, Release build.

    def __init__(self, testFunc):
        host = MockHost()
        self._port = host.port_factory.get('test-win-win7', None)
        self._exp = None
        unittest.TestCase.__init__(self, testFunc)

    def get_basic_tests(self):
        return ['failures/expected/text.html',
                'failures/expected/image_checksum.html',
                'failures/expected/crash.html',
                'failures/expected/image.html',
                'failures/expected/timeout.html',
                'failures/unexpected/*/text.html',
                'passes/text.html',
                'reftests/failures/expected/has_unused_expectation.html']

    def get_basic_expectations(self):
        return """
# results: [ Failure Crash ]
failures/expected/text.html [ Failure ]
failures/expected/crash.html [ Crash ]
failures/expected/image_checksum.html [ Crash ]
failures/expected/image.html [ Crash ]
"""

    def parse_exp(self, expectations, overrides=None, is_lint_mode=False):
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = expectations
        if overrides:
            expectations_dict['overrides'] = overrides
        self._port.expectations_dict = lambda: expectations_dict
        expectations_to_lint = expectations_dict if is_lint_mode else None
        self._exp = TestExpectations(self._port, expectations_dict=expectations_to_lint)

    def assert_exp_list(self, test, results):
        self.assertEqual(self._exp.get_expectations(test).results, set(results))

    def assert_exp(self, test, result):
        self.assert_exp_list(test, [result])

    def assert_bad_expectations(self, expectations, overrides=None):
        with self.assertRaises(ParseError):
            self.parse_exp(expectations, is_lint_mode=True, overrides=overrides)


class BasicTests(Base):

    def test_basic(self):
        self.parse_exp(self.get_basic_expectations())
        self.assert_exp('failures/expected/text.html', ResultType.Failure)
        self.assert_exp_list('failures/expected/image_checksum.html', [ResultType.Crash])
        self.assert_exp('passes/text.html', ResultType.Pass)
        self.assert_exp('failures/expected/image.html', ResultType.Crash)


class SystemConfigurationRemoverTests(Base):

    def __init__(self, testFunc):
        super(SystemConfigurationRemoverTests, self).__init__(testFunc)
        self._port.configuration_specifier_macros_dict = {
            'mac': ['mac10.10', 'mac10.11', 'mac10.12'],
            'win': ['win7', 'win10'],
            'linux': ['precise', 'trusty']
        }

    def set_up_using_raw_expectations(self, content):
        self._general_exp_filename = 'TestExpectations'
        self._port.host.filesystem.write_text_file(self._general_exp_filename, content)
        expectations_dict = {self._general_exp_filename: content}
        test_expectations = TestExpectations(self._port, expectations_dict)
        self._system_config_remover = SystemConfigurationRemover(test_expectations)

    def test_remove_mac_version_from_mac_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be split\n'
            '[ Mac ] failures/expected/text.html?\* [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        self._system_config_remover.remove_os_versions('failures/expected/text.html?*', set(['Mac10.10']))
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(self._general_exp_filename)
        self.assertEqual(updated_exps, (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be split\n'
            '[ Mac10.11 ] failures/expected/text.html?\* [ Failure ]\n'
            '[ Mac10.12 ] failures/expected/text.html?\* [ Failure ]\n'))

    def test_remove_mac_version_from_linux_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Linux ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be unaffected\n'
            '[ Linux ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        self._system_config_remover.remove_os_versions('failures/expected/text.html', set(['Mac10.10']))
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(self._general_exp_filename)
        self.assertEqual(updated_exps, raw_expectations)

    def test_remove_mac_version_from_all_config_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be split\n'
            'failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        self._system_config_remover.remove_os_versions('failures/expected/text.html', set(['Mac10.10']))
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(self._general_exp_filename)
        self.assertEqual(updated_exps, (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be split\n'
            '[ Linux ] failures/expected/text.html [ Failure ]\n'
            '[ Mac10.11 ] failures/expected/text.html [ Failure ]\n'
            '[ Mac10.12 ] failures/expected/text.html [ Failure ]\n'
            '[ Win ] failures/expected/text.html [ Failure ]\n'))

    def test_remove_all_mac_versions_from_mac_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]\n'
            '# results: [ Failure ]\n'
            '# The expectation below and this comment block should be deleted\n'
            '[ Mac ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        self._system_config_remover.remove_os_versions(
            'failures/expected/text.html', {'Mac10.10', 'Mac10.11', 'Mac10.12'})
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(self._general_exp_filename)
        self.assertEqual(updated_exps, (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]\n'
            '# results: [ Failure ]\n'))

    def test_remove_all_mac_versions_from_all_config_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be split\n'
            'failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        self._system_config_remover.remove_os_versions(
            'failures/expected/text.html', {'Mac10.10', 'Mac10.11', 'Mac10.12'})
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(self._general_exp_filename)
        self.assertEqual(updated_exps, (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be split\n'
            '[ Linux ] failures/expected/text.html [ Failure ]\n'
            '[ Win ] failures/expected/text.html [ Failure ]\n'))

    def test_remove_all_mac_versions_from_linux_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be unaffected\n'
            '[ Linux ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        self._system_config_remover.remove_os_versions(
            'failures/expected/text.html', {'Mac10.10', 'Mac10.11', 'Mac10.12'})
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(self._general_exp_filename)
        self.assertEqual(updated_exps, raw_expectations)

    def test_remove_all_configs(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation and this comment should be deleted\n'
            'failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        all_versions = reduce(lambda x, y: x+y, self._port.configuration_specifier_macros_dict.values())
        self._system_config_remover.remove_os_versions(
            'failures/expected/text.html', all_versions)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(self._general_exp_filename)
        self.assertEqual(updated_exps, (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'))

    def test_remove_all_configs2(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation and this comment should be deleted\n'
            '[ Mac ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        all_versions = reduce(lambda x, y: x+y, self._port.configuration_specifier_macros_dict.values())
        self._system_config_remover.remove_os_versions(
            'failures/expected/text.html', all_versions)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(self._general_exp_filename)
        self.assertEqual(updated_exps, (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'))

    def test_remove_mac_version_from_another_mac_version_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Linux ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be unaffected\n'
            '[ Mac10.11 ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        self._system_config_remover.remove_os_versions('failures/expected/text.html', set(['Mac10.10']))
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(self._general_exp_filename)
        self.assertEqual(updated_exps, raw_expectations)

    def test_remove_mac_version_from_same_mac_version_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Linux ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation as well as this comment should be deleted\n'
            '[ Mac10.10 ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        self._system_config_remover.remove_os_versions('failures/expected/text.html', set(['Mac10.10']))
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(self._general_exp_filename)
        self.assertEqual(updated_exps, (
            '# tags: [ Mac10.10 Mac10.11 Linux ]\n'
            '# results: [ Failure ]\n'))


class MiscTests(Base):

    def test_parse_mac_legacy_names(self):
        host = MockHost()
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = ('# tags: [ Mac10.10 ]\n'
                                             '# results: [ Failure ]\n'
                                             '[ Mac10.10 ] failures/expected/text.html [ Failure ]\n')

        port = host.port_factory.get('test-mac-mac10.10', None)
        port.expectations_dict = lambda: expectations_dict
        expectations = TestExpectations(port)
        self.assertEqual(expectations.get_expectations('failures/expected/text.html').results, {ResultType.Failure})

        port = host.port_factory.get('test-win-win7', None)
        port.expectations_dict = lambda: expectations_dict
        expectations = TestExpectations(port)
        self.assertEqual(expectations.get_expectations('failures/expected/text.html').results, {ResultType.Pass})

    def test_get_test_with_expected_result(self):
        test_expectations = ('# tags: [ win7 linux ]\n'
                             '# results: [ Failure ]\n'
                             '[ win7 ] failures/expected/text.html [ Failure ]\n'
                             '[ linux ] failures/expected/image_checksum.html [ Failure ]\n')
        self.parse_exp(test_expectations)
        self.assertEqual(self._exp.get_tests_with_expected_result(ResultType.Failure),
                         set(['failures/expected/text.html']))

    def test_multiple_results(self):
        self.parse_exp('# results: [ Crash Failure ]\nfailures/expected/text.html [ Crash Failure ]')
        self.assertEqual(self._exp.get_expectations('failures/expected/text.html').results, {ResultType.Failure, ResultType.Crash})

    def test_overrides_include_slow(self):
        self.parse_exp('# results: [ Failure ]\nfailures/expected/text.html [ Failure ]',
                       '# results: [ Slow ]\nfailures/expected/text.html [ Slow ]')
        self.assert_exp_list('failures/expected/text.html', set([ResultType.Failure]))
        self.assertTrue(self._exp.get_expectations('failures/expected/text.html').is_slow_test)

    def test_overrides(self):
        self.parse_exp('# results: [ Failure ]\nfailures/expected/text.html [ Failure ]',
                       '# results: [ Timeout ]\nfailures/expected/text.html [ Timeout ]')
        self.assert_exp_list('failures/expected/text.html', {ResultType.Failure, ResultType.Timeout})

    def test_more_specific_override_resets_skip(self):
        self.parse_exp('# results: [ Skip ]\nfailures/expected* [ Skip ]',
                       '# results: [ Failure ]\nfailures/expected/text.html [ Failure ]')
        self.assert_exp_list('failures/expected/text.html', {ResultType.Failure, ResultType.Skip})

    def test_bot_test_expectations(self):
        """Test that expectations are merged rather than overridden when using flaky option 'unexpected'."""
        test_name1 = 'failures/expected/text.html'
        test_name2 = 'passes/text.html'

        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = (
            '# results: [ Failure Crash ]\n%s [ Failure ]\n%s [ Crash ]\n' % (test_name1, test_name2))
        self._port.expectations_dict = lambda: expectations_dict

        expectations = TestExpectations(self._port)
        self.assertEqual(expectations.get_expectations(test_name1).results, set([ResultType.Failure]))
        self.assertEqual(expectations.get_expectations(test_name2).results, set([ResultType.Crash]))

        def bot_expectations():
            return {test_name1: [ResultType.Pass, ResultType.Timeout], test_name2: [ResultType.Crash]}
        self._port.bot_expectations = bot_expectations
        self._port._options.ignore_flaky_tests = 'unexpected'

        expectations = TestExpectations(self._port)
        self.assertEqual(expectations.get_expectations(test_name1).results,
                         set([ResultType.Pass, ResultType.Failure, ResultType.Timeout]))
        self.assertEqual(expectations.get_expectations(test_name2).results, set([ResultType.Crash]))


class SkippedTests(Base):

    def check(self, expectations, overrides, ignore_tests, lint=False, expected_results=None):
        expected_results = expected_results or [ResultType.Skip, ResultType.Failure]
        port = MockHost().port_factory.get(
            'test-win-win7',
            options=optparse.Values({'ignore_tests': ignore_tests}))
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(
                port.web_tests_dir(), 'failures/expected/text.html'),
            'foo')
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = expectations
        if overrides:
            expectations_dict['overrides'] = overrides
        port.expectations_dict = lambda: expectations_dict
        expectations_to_lint = expectations_dict if lint else None
        exp = TestExpectations(port, expectations_dict=expectations_to_lint)
        self.assertEqual(exp.get_expectations('failures/expected/text.html').results, set(expected_results))

    def test_skipped_file_overrides_expectations(self):
        self.check(expectations='# results: [ Failure Skip ]\nfailures/expected/text.html [ Failure ]\n',
                   overrides=None, ignore_tests=['failures/expected/text.html'])

    def test_skipped_file_overrides_overrides(self):
        self.check(expectations='# results: [ Skip Failure ]\n',
                   overrides='# results: [ Skip Failure ]\nfailures/expected/text.html [ Failure ]\n',
                   ignore_tests=['failures/expected/text.html'])


class PrecedenceTests(Base):

    def test_file_over_directory(self):
        # This tests handling precedence of specific lines over directories
        # and tests expectations covering entire directories.
        exp_str = """
# results: [ Failure Crash ]
failures/expected/text.html [ Failure ]
failures/expected* [ Crash ]
"""
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', ResultType.Failure)
        self.assert_exp_list('failures/expected/crash.html', [ResultType.Crash])

        exp_str = """
# results: [ Failure Crash ]
failures/expected* [ Crash ]
failures/expected/text.html [ Failure ]
"""
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', ResultType.Failure)
        self.assert_exp_list('failures/expected/crash.html', [ResultType.Crash])

