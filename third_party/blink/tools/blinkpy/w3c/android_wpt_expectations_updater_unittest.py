# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.git_cl import TryJobStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.net.web_test_results import WebTestResults
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.web_tests.port.android import PRODUCTS_TO_EXPECTATION_FILE_PATHS
from blinkpy.w3c.android_wpt_expectations_updater import (
    AndroidWPTExpectationsUpdater)


class AndroidWPTExpectationsUpdaterTest(LoggingTestCase):

    _raw_android_expectations = (
        '# results: [ Failure Crash Timeout]\n'
        '\n'
        '# Add untriaged failures in this block\n'
        'external/wpt/abc.html [ Failure ]\n'
        'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
        'crbug.com/1050754 external/wpt/ghi.html [ Timeout ]\n'
        'crbug.com/1111111 external/wpt/jkl.html [ Failure ]\n'
        'external/wpt/www.html [ Crash Failure ]\n'
        'crbug.com/1050754 external/wpt/cat.html [ Failure ]\n'
        'external/wpt/dog.html [ Crash Timeout ]\n'
        'crbug.com/6789043 external/wpt/van.html [ Failure ]\n'
        '\n'
        '# This comment will not be deleted\n'
        'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n')

    def _setup_host(self):
        """Returns a mock host with fake values set up for testing."""
        self.set_logging_level(logging.DEBUG)
        host = MockHost()
        host.port_factory = MockPortFactory(host)

        # Set up a fake list of try builders.
        host.builders = BuilderList({
            'MOCK Try Precise': {
                'port_name': 'test-linux-precise',
                'specifiers': ['Precise', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Android Weblayer - Pie': {
                'port_name': 'test-android-pie',
                'specifiers': ['Precise', 'Release',
                               'anDroid', 'android_Weblayer'],
                'is_try_builder': True,
            },
            'MOCK Android Pie': {
                'port_name': 'test-android-pie',
                'specifiers': ['Precise', 'Release', 'anDroid',
                               'Android_Webview', 'Chrome_Android'],
                'is_try_builder': True,
            },
        })
        # Write dummy expectations
        for path in PRODUCTS_TO_EXPECTATION_FILE_PATHS.values():
            host.filesystem.write_text_file(
                path, self._raw_android_expectations)
        return host

    def testUpdateTestExpectationsForWebview(self):
        host = self._setup_host()
        host.results_fetcher.set_results(
            Build('MOCK Android Pie', 123),
            WebTestResults({
                'tests': {
                    'abc.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                    'jkl.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                    },
                    'cat.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                },
            }, step_name='system_webview_wpt (with patch)'),
            step_name='system_webview_wpt (with patch)')
        updater = AndroidWPTExpectationsUpdater(
            host, ['-vvv',  '--android-product', 'android_webview'])
        updater.git_cl = MockGitCL(host, {
            Build('MOCK Android Pie', 123):
            TryJobStatus('COMPLETED', 'FAILURE')})
        # Run command
        updater.run()
        # Get new expectations
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS['android_webview'])
        self.assertEqual(
            content,
            ('# results: [ Failure Crash Timeout]\n'
             '\n'
             '# Add untriaged failures in this block\n'
             'crbug.com/1050754 external/wpt/abc.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/cat.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
             'external/wpt/dog.html [ Crash Timeout ]\n'
             'crbug.com/1050754 external/wpt/ghi.html [ Timeout ]\n'
             'crbug.com/1111111 crbug.com/1050754'
             ' external/wpt/jkl.html [ Failure ]\n'
             'crbug.com/6789043 external/wpt/van.html [ Failure ]\n'
             'external/wpt/www.html [ Crash Failure ]\n'
             '\n'
             '# This comment will not be deleted\n'
             'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n'))
        # check that chrome android's expectation file was not modified
        # since the same bot is used to update chrome android & webview
        # expectations
        self.assertEqual(
            host.filesystem.read_text_file(
                PRODUCTS_TO_EXPECTATION_FILE_PATHS['chrome_android']),
            self._raw_android_expectations)
        # Check logs
        logs = ''.join(self.logMessages()).lower()
        self.assertNotIn('weblayer', logs)
        self.assertNotIn('chrome', logs)

    def testUpdateTestExpectationsForWeblayer(self):
        host = self._setup_host()
        host.results_fetcher.set_results(
            Build('MOCK Android Weblayer - Pie', 123),
            WebTestResults({
                'tests': {
                    'abc.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                    'jkl.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                    },
                    'cat.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                    'new.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH CRASH FAIL',
                        'is_unexpected': True,
                    },
                },
            }, step_name='weblayer_shell_wpt (with patch)'),
            step_name='weblayer_shell_wpt (with patch)')
        updater = AndroidWPTExpectationsUpdater(
            host, ['-vvv', '--android-product', 'android_weblayer'])
        updater.git_cl = MockGitCL(host, {
            Build('MOCK Android Weblayer - Pie', 123):
            TryJobStatus('COMPLETED', 'FAILURE')})
        # Run command
        updater.run()
        # Get new expectations
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS['android_weblayer'])
        self.assertEqual(
            content,
            ('# results: [ Failure Crash Timeout]\n'
             '\n'
             '# Add untriaged failures in this block\n'
             'crbug.com/1050754 external/wpt/abc.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/cat.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
             'external/wpt/dog.html [ Crash Timeout ]\n'
             'crbug.com/1050754 external/wpt/ghi.html [ Timeout ]\n'
             'crbug.com/1111111 crbug.com/1050754'
             ' external/wpt/jkl.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/new.html [ Failure Crash ]\n'
             'crbug.com/6789043 external/wpt/van.html [ Failure ]\n'
             'external/wpt/www.html [ Crash Failure ]\n'
             '\n'
             '# This comment will not be deleted\n'
             'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n'))
        # Check logs
        logs = ''.join(self.logMessages()).lower()
        self.assertNotIn('webview', logs)
        self.assertNotIn('chrome', logs)

    def testUpdateTestExpectationsForAll(self):
        host = self._setup_host()
        # Add results for Weblayer
        host.results_fetcher.set_results(
            Build('MOCK Android Weblayer - Pie', 123),
            WebTestResults({
                'tests': {
                    'abc.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                    'weblayer_only.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH CRASH FAIL',
                        'is_unexpected': True,
                    },
                },
            }, step_name='weblayer_shell_wpt (with patch)'),
            step_name='weblayer_shell_wpt (with patch)')
        # Add Results for Webview
        host.results_fetcher.set_results(
            Build('MOCK Android Pie', 101),
            WebTestResults({
                'tests': {
                    'cat.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH FAIL TIMEOUT',
                        'is_unexpected': True,
                    },
                    'webview_only.html': {
                        'expected': 'PASS',
                        'actual': 'TIMEOUT',
                        'is_unexpected': True,
                    },
                },
            }, step_name='system_webview_wpt (with patch)'),
            step_name='system_webview_wpt (with patch)')
        # Add Results for Chrome
        host.results_fetcher.set_results(
            Build('MOCK Android Pie', 101),
            WebTestResults({
                'tests': {
                    'jkl.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'is_unexpected': True,
                    },
                    'chrome_only.html': {
                        'expected': 'PASS',
                        'actual': 'CRASH CRASH TIMEOUT',
                        'is_unexpected': True,
                    },
                },
            }, step_name='chrome_public_wpt (with patch)'),
            step_name='chrome_public_wpt (with patch)')
        updater = AndroidWPTExpectationsUpdater(
            host, ['-vvv',
                   '--android-product', 'android_weblayer',
                   '--android-product', 'chrome_android',
                   '--android-product', 'android_webview'])
        updater.git_cl = MockGitCL(host, {
            Build('MOCK Android Weblayer - Pie', 123):
            TryJobStatus('COMPLETED', 'FAILURE'),
            Build('MOCK Android Pie', 101):
            TryJobStatus('COMPLETED', 'FAILURE')})
        # Run command
        updater.run()
        # Check expectations for weblayer
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS['android_weblayer'])
        self.assertEqual(
            content,
            ('# results: [ Failure Crash Timeout]\n'
             '\n'
             '# Add untriaged failures in this block\n'
             'crbug.com/1050754 external/wpt/abc.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/cat.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
             'external/wpt/dog.html [ Crash Timeout ]\n'
             'crbug.com/1050754 external/wpt/ghi.html [ Timeout ]\n'
             'crbug.com/1111111 external/wpt/jkl.html [ Failure ]\n'
             'crbug.com/6789043 external/wpt/van.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/weblayer_only.html [ Failure Crash ]\n'
             'external/wpt/www.html [ Crash Failure ]\n'
             '\n'
             '# This comment will not be deleted\n'
             'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n'))
        # Check expectations for webview
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS['android_webview'])
        self.assertEqual(
            content,
            ('# results: [ Failure Crash Timeout]\n'
             '\n'
             '# Add untriaged failures in this block\n'
             'external/wpt/abc.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/cat.html [ Crash Failure Timeout ]\n'
             'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
             'external/wpt/dog.html [ Crash Timeout ]\n'
             'crbug.com/1050754 external/wpt/ghi.html [ Timeout ]\n'
             'crbug.com/1111111 external/wpt/jkl.html [ Failure ]\n'
             'crbug.com/6789043 external/wpt/van.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/webview_only.html [ Timeout ]\n'
             'external/wpt/www.html [ Crash Failure ]\n'
             '\n'
             '# This comment will not be deleted\n'
             'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n'))
        # Check expectations chrome
        content = host.filesystem.read_text_file(
            PRODUCTS_TO_EXPECTATION_FILE_PATHS['chrome_android'])
        self.assertEqual(
            content,
            ('# results: [ Failure Crash Timeout]\n'
             '\n'
             '# Add untriaged failures in this block\n'
             'external/wpt/abc.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/cat.html [ Failure ]\n'
             'crbug.com/1050754 external/wpt/chrome_only.html [ Crash Timeout ]\n'
             'crbug.com/1050754 external/wpt/def.html [ Crash ]\n'
             'external/wpt/dog.html [ Crash Timeout ]\n'
             'crbug.com/1050754 external/wpt/ghi.html [ Timeout ]\n'
             'crbug.com/1111111 crbug.com/1050754'
             ' external/wpt/jkl.html [ Failure ]\n'
             'crbug.com/6789043 external/wpt/van.html [ Failure ]\n'
             'external/wpt/www.html [ Crash Failure ]\n'
             '\n'
             '# This comment will not be deleted\n'
             'crbug.com/111111 external/wpt/hello_world.html [ Crash ]\n'))
