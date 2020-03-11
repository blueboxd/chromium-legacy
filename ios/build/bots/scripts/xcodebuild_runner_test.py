# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for xcodebuild_runner.py."""

import logging
import mock
import os
import shutil
import unittest

import iossim_util
import plistlib
import tempfile
import test_apps
import test_runner
import test_runner_test
import xcode_log_parser
import xcodebuild_runner


_ROOT_FOLDER_PATH = 'root/folder'
_XCODE_BUILD_VERSION = '10B61'
_DESTINATION = 'A4E66321-177A-450A-9BA1-488D85B7278E'
_OUT_DIR = 'out/dir'
_XTEST_RUN = '/tmp/temp_file.xctestrun'
_EGTESTS_APP_PATH = '%s/any_egtests.app' % _ROOT_FOLDER_PATH


class XCodebuildRunnerTest(test_runner_test.TestCase):
  """Test case to test xcodebuild_runner."""

  def setUp(self):
    super(XCodebuildRunnerTest, self).setUp()
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(xcode_log_parser.XcodeLogParser, 'copy_screenshots',
              lambda _1, _2: None)
    self.mock(os, 'listdir', lambda _: ['any_egtests.xctest'])
    self.mock(xcodebuild_runner, 'get_all_tests',
              lambda _1, _2: ['Class1/passedTest1', 'Class1/passedTest2'])
    self.tmpdir = tempfile.mkdtemp()
    self.mock(iossim_util, 'get_simulator_list',
              lambda: test_runner_test.SIMULATORS_LIST)
    self.mock(test_apps, 'get_bundle_id', lambda _: "fake-bundle-id")

  def tearDown(self):
    shutil.rmtree(self.tmpdir, ignore_errors=True)
    super(XCodebuildRunnerTest, self).tearDown()

  def fake_launch_attempt(self, obj, statuses):
    attempt = [0]
    info_plist_statuses = {
        'not_started': {
            'Actions': [{'ActionResult': {}}],
            'TestsCount': 0, 'TestsFailedCount': 0
        },
        'fail': {
            'Actions': [
                {'ActionResult': {
                    'TestSummaryPath': '1_Test/TestSummaries.plist'}
                }
            ],
            'TestsCount': 99,
            'TestsFailedCount': 1},
        'pass': {
            'Actions': [
                {'ActionResult': {
                    'TestSummaryPath': '1_Test/TestSummaries.plist'}
                }
            ],
            'TestsCount': 100,
            'TestsFailedCount': 0}
    }

    test_summary = {
        'TestableSummaries': [
            {'TargetName': 'egtests',
             'Tests': [
                 {'Subtests': [
                     {'Subtests': [
                         {'Subtests': [
                             {'TestIdentifier': 'passed_test',
                              'TestStatus': 'Success'
                             }
                         ]
                         }
                     ]
                     }
                 ]
                 }
             ]
            }
        ]
        }

    def the_fake(cmd):
      index = attempt[0]
      attempt[0] += 1
      attempt_outdir = os.path.join(self.tmpdir, 'attempt_%d' % index)
      self.assertEqual(1, cmd.count(attempt_outdir))
      os.mkdir(attempt_outdir)
      with open(os.path.join(attempt_outdir, 'Info.plist'), 'w') as f:
        plistlib.writePlist(info_plist_statuses[statuses[index]], f)
      summary_folder = os.path.join(attempt_outdir, '1_Test')
      os.mkdir(summary_folder)
      with open(os.path.join(summary_folder, 'TestSummaries.plist'), 'w') as f:
        plistlib.writePlist(test_summary, f)
      return (-6, 'Output for attempt_%d' % index)

    obj.launch_attempt = the_fake

  def testEgtests_not_found_egtests_app(self):
    self.mock(os.path, 'exists', lambda _: False)
    with self.assertRaises(test_runner.AppNotFoundError):
      test_apps.EgtestsApp(_EGTESTS_APP_PATH)

  def testEgtests_not_found_plugins(self):
    egtests = test_apps.EgtestsApp(_EGTESTS_APP_PATH)
    self.mock(os.path, 'exists', lambda _: False)
    with self.assertRaises(test_runner.PlugInsNotFoundError):
      egtests._xctest_path()

  def testEgtests_found_xctest(self):
    self.assertEqual('/PlugIns/any_egtests.xctest',
                     test_apps.EgtestsApp(_EGTESTS_APP_PATH)._xctest_path())

  @mock.patch('os.listdir', autospec=True)
  def testEgtests_not_found_xctest(self, mock_listdir):
    mock_listdir.return_value = ['random_file']
    egtest = test_apps.EgtestsApp(_EGTESTS_APP_PATH)
    with self.assertRaises(test_runner.XCTestPlugInNotFoundError):
      egtest._xctest_path()

  def testEgtests_xctestRunNode_without_filter(self):
    egtest_node = test_apps.EgtestsApp(
        _EGTESTS_APP_PATH).fill_xctestrun_node()['any_egtests_module']
    self.assertNotIn('OnlyTestIdentifiers', egtest_node)
    self.assertNotIn('SkipTestIdentifiers', egtest_node)

  def testEgtests_xctestRunNode_with_filter_only_identifiers(self):
    filtered_tests = ['TestCase1/testMethod1', 'TestCase1/testMethod2',
                      'TestCase2/testMethod1', 'TestCase1/testMethod2']
    egtest_node = test_apps.EgtestsApp(
        _EGTESTS_APP_PATH, included_tests=filtered_tests).fill_xctestrun_node(
        )['any_egtests_module']
    self.assertEqual(filtered_tests, egtest_node['OnlyTestIdentifiers'])
    self.assertNotIn('SkipTestIdentifiers', egtest_node)

  def testEgtests_xctestRunNode_with_filter_skip_identifiers(self):
    skipped_tests = ['TestCase1/testMethod1', 'TestCase1/testMethod2',
                     'TestCase2/testMethod1', 'TestCase1/testMethod2']
    egtest_node = test_apps.EgtestsApp(
        _EGTESTS_APP_PATH, excluded_tests=skipped_tests).fill_xctestrun_node(
        )['any_egtests_module']
    self.assertEqual(skipped_tests, egtest_node['SkipTestIdentifiers'])
    self.assertNotIn('OnlyTestIdentifiers', egtest_node)

  @mock.patch('test_runner.get_current_xcode_info', autospec=True)
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def testLaunchCommand_restartFailed1stAttempt(self, mock_collect_results,
                                                xcode_version):
    egtests = test_apps.EgtestsApp(_EGTESTS_APP_PATH)
    xcode_version.return_value = {'version': '10.2.1'}
    mock_collect_results.side_effect = [
        {'failed': {'TESTS_DID_NOT_START': ['not started']}, 'passed': []},
        {'failed': {}, 'passed': ['Class1/passedTest1', 'Class1/passedTest2']}
    ]
    launch_command = xcodebuild_runner.LaunchCommand(egtests,
                                                     _DESTINATION,
                                                     shards=1,
                                                     retries=3,
                                                     out_dir=self.tmpdir)
    self.fake_launch_attempt(launch_command, ['not_started', 'pass'])
    launch_command.launch()
    self.assertEqual(1, len(launch_command.test_results))

  @mock.patch('test_runner.get_current_xcode_info', autospec=True)
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def testLaunchCommand_notRestartPassedTest(self, mock_collect_results,
                                             xcode_version):
    egtests = test_apps.EgtestsApp(_EGTESTS_APP_PATH)
    xcode_version.return_value = {'version': '10.2.1'}
    mock_collect_results.side_effect = [
        {'failed': {'BUILD_INTERRUPTED': 'BUILD_INTERRUPTED: attempt # 0'},
         'passed': ['Class1/passedTest1', 'Class1/passedTest2']}
    ]
    launch_command = xcodebuild_runner.LaunchCommand(egtests,
                                                     _DESTINATION,
                                                     shards=1,
                                                     retries=3,
                                                     out_dir=self.tmpdir)
    self.fake_launch_attempt(launch_command, ['pass'])
    launch_command.launch()
    self.assertEqual(1, len(launch_command.test_results['attempts']))


class DeviceXcodeTestRunnerTest(test_runner_test.TestCase):
  """Test case to test xcodebuild_runner.DeviceXcodeTestRunner."""

  def setUp(self):
    super(DeviceXcodeTestRunnerTest, self).setUp()
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(test_runner, 'get_current_xcode_info', lambda: {
        'version': 'test version', 'build': 'test build', 'path': 'test/path'})
    self.mock(os.path, 'abspath', lambda path: '/abs/path/to/%s' % path)

    self.mock(test_runner.subprocess, 'check_output', lambda _: 'fake-output')
    self.mock(test_runner.subprocess, 'check_call', lambda _: 'fake-out')
    self.mock(test_runner.TestRunner, 'set_sigterm_handler',
              lambda self, handler: 0)
    self.mock(os, 'listdir', lambda _: [])
    self.mock(test_runner, 'is_iOS13_or_higher_device', lambda _: False)
    self.mock(test_runner, 'print_process_output', lambda _: [])
    self.mock(test_runner.TestRunner, 'start_proc', lambda self, cmd: 0)
    self.mock(test_runner.DeviceTestRunner, 'get_installed_packages',
              lambda self: [])
    self.mock(test_runner.DeviceTestRunner, 'wipe_derived_data', lambda _: None)
    self.mock(test_runner.TestRunner, 'retrieve_derived_data', lambda _: None)

  def test_launch(self):
    """Tests launch method in DeviceXcodeTestRunner"""
    self.mock(xcodebuild_runner.pool.ThreadPool, 'imap_unordered',
              lambda _1, _2, _3: [])
    self.mock(xcodebuild_runner, 'get_all_tests', lambda _1, _2: [])
    tr = xcodebuild_runner.DeviceXcodeTestRunner(
        "fake-app-path", "fake-host-app-path", "fake-out-dir")
    self.assertTrue(tr.launch())

  def test_tear_down(self):
    tr = xcodebuild_runner.DeviceXcodeTestRunner(
        "fake-app-path", "fake-host-app-path", "fake-out-dir")
    tr.tear_down()


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s:%(levelname)s] %(message)s',
      level=logging.DEBUG,
      datefmt='%I:%M:%S')
  unittest.main()
