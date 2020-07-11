# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# It's reasonable for unittests to be messing with protected members.
# pylint: disable=protected-access

import json
import os
import unittest
import mock
import gpu_project_config
import run_gpu_integration_test
import tempfile

from gpu_tests import context_lost_integration_test
from gpu_tests import gpu_helper
from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import webgl_conformance_integration_test

from py_utils import tempfile_ext

from telemetry.internal.util import binary_manager
from telemetry.internal.platform import system_info
from telemetry.testing import browser_test_runner
from telemetry.testing import fakes
from telemetry.testing import run_browser_tests

path_util.AddDirToPathIfNeeded(path_util.GetChromiumSrcDir(), 'tools', 'perf')
from chrome_telemetry_build import chromium_config

VENDOR_NVIDIA = 0x10DE
VENDOR_AMD = 0x1002
VENDOR_INTEL = 0x8086

VENDOR_STRING_IMAGINATION = 'Imagination Technologies'
DEVICE_STRING_SGX = 'PowerVR SGX 554'


def _GetSystemInfo(gpu='',
                   device='',
                   vendor_string='',
                   device_string='',
                   passthrough=False,
                   gl_renderer=''):
  sys_info = {
      'model_name': '',
      'gpu': {
          'devices': [
              {
                  'vendor_id': gpu,
                  'device_id': device,
                  'vendor_string': vendor_string,
                  'device_string': device_string
              },
          ],
          'aux_attributes': {
              'passthrough_cmd_decoder': passthrough
          }
      }
  }
  if gl_renderer:
    sys_info['gpu']['aux_attributes']['gl_renderer'] = gl_renderer
  return system_info.SystemInfo.FromDict(sys_info)


def _GetTagsToTest(browser, test_class=None):
  test_class = test_class or gpu_integration_test.GpuIntegrationTest
  tags = None
  with mock.patch.object(
      test_class, 'ExpectationsFiles', return_value=['exp.txt']):
    tags = set(test_class.GetPlatformTags(browser))
  return tags


def _GenerateNvidiaExampleTagsForTestClassAndArgs(test_class, args):
  tags = None
  with mock.patch.object(
      test_class, 'ExpectationsFiles', return_value=['exp.txt']):
    _ = [_ for _ in test_class.GenerateGpuTests(args)]
    platform = fakes.FakePlatform('win', 'win10')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        gpu=VENDOR_NVIDIA, device=0x1cb3, gl_renderer='ANGLE Direct3D9')
    tags = _GetTagsToTest(browser, test_class)
  return tags


class GpuIntegrationTestUnittest(unittest.TestCase):
  def setUp(self):
    self._test_state = {}
    self._test_result = {}

  def _RunGpuIntegrationTests(self, test_name, extra_args=None):
    extra_args = extra_args or []
    unittest_config = chromium_config.ChromiumConfig(
        top_level_dir=path_util.GetGpuTestDir(),
        benchmark_dirs=[
            os.path.join(path_util.GetGpuTestDir(), 'unittest_data')
        ])
    with binary_manager.TemporarilyReplaceBinaryManager(None), \
         mock.patch.object(gpu_project_config, 'CONFIG', unittest_config):
      # TODO(crbug.com/1103792): Using NamedTemporaryFile() as a generator is
      # causing windows bots to fail. When the issue is fixed with
      # tempfile_ext.NamedTemporaryFile(), put it in the list of generators
      # starting this with block. Also remove the try finally statement
      # below.
      temp_file = tempfile.NamedTemporaryFile(delete=False)
      temp_file.close()
      try:
        test_argv = [
            test_name, '--write-full-results-to=%s' % temp_file.name
        ] + extra_args
        processed_args = run_gpu_integration_test.ProcessArgs(test_argv)
        telemetry_args = browser_test_runner.ProcessConfig(
            unittest_config, processed_args)
        run_browser_tests.RunTests(telemetry_args)
        with open(temp_file.name) as f:
          self._test_result = json.load(f)
      finally:
        temp_file.close()

  def testOverrideDefaultRetryArgumentsinRunGpuIntegrationTests(self):
    self._RunGpuIntegrationTests('run_tests_with_expectations_files',
                                 ['--retry-limit=1'])
    self.assertEqual(
        self._test_result['tests']['a']['b']['unexpected-fail.html']['actual'],
        'FAIL FAIL')

  def testDefaultRetryArgumentsinRunGpuIntegrationTests(self):
    self._RunGpuIntegrationTests('run_tests_with_expectations_files')
    self.assertEqual(
        self._test_result['tests']['a']['b']['expected-flaky.html']['actual'],
        'FAIL FAIL FAIL')

  def testTestNamePrefixGenerationInRunGpuIntegrationTests(self):
    self._RunGpuIntegrationTests('simple_integration_unittest')
    self.assertIn('expected_failure', self._test_result['tests'])

  def _TestTagGenerationForMockPlatform(self, test_class, args):
    tag_set = _GenerateNvidiaExampleTagsForTestClassAndArgs(test_class, args)
    self.assertTrue(
        set([
            'win', 'win10', 'd3d9', 'release', 'nvidia', 'nvidia-0x1cb3',
            'no-passthrough'
        ]).issubset(tag_set))
    return tag_set

  def testGenerateContextLostExampleTagsForAsan(self):
    args = gpu_helper.GetMockArgs(is_asan=True)
    tag_set = self._TestTagGenerationForMockPlatform(
        context_lost_integration_test.ContextLostIntegrationTest, args)
    self.assertIn('asan', tag_set)
    self.assertNotIn('no-asan', tag_set)

  def testGenerateContextLostExampleTagsForNoAsan(self):
    args = gpu_helper.GetMockArgs()
    tag_set = self._TestTagGenerationForMockPlatform(
        context_lost_integration_test.ContextLostIntegrationTest, args)
    self.assertIn('no-asan', tag_set)
    self.assertNotIn('asan', tag_set)

  def testGenerateWebglConformanceExampleTagsForWebglVersion1andAsan(self):
    args = gpu_helper.GetMockArgs(is_asan=True, webgl_version='1.0.0')
    tag_set = self._TestTagGenerationForMockPlatform(
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest,
        args)
    self.assertTrue(set(['asan', 'webgl-version-1']).issubset(tag_set))
    self.assertFalse(set(['no-asan', 'webgl-version-2']) & tag_set)

  def testGenerateWebglConformanceExampleTagsForWebglVersion2andNoAsan(self):
    args = gpu_helper.GetMockArgs(is_asan=False, webgl_version='2.0.0')
    tag_set = self._TestTagGenerationForMockPlatform(
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest,
        args)
    self.assertTrue(set(['no-asan', 'webgl-version-2']).issubset(tag_set))
    self.assertFalse(set(['asan', 'webgl-version-1']) & tag_set)

  def testGenerateNvidiaExampleTags(self):
    platform = fakes.FakePlatform('win', 'win10')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        gpu=VENDOR_NVIDIA, device=0x1cb3, gl_renderer='ANGLE Direct3D9')
    self.assertEqual(
        _GetTagsToTest(browser),
        set([
            'win', 'win10', 'release', 'nvidia', 'nvidia-0x1cb3', 'd3d9',
            'no-passthrough', 'no-swiftshader-gl', 'no-use-vulkan',
            'no-skia-renderer'
        ]))

  def testGenerateVendorTagUsingVendorString(self):
    platform = fakes.FakePlatform('mac', 'mojave')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        vendor_string=VENDOR_STRING_IMAGINATION,
        device_string=DEVICE_STRING_SGX,
        passthrough=True,
        gl_renderer='ANGLE OpenGL ES')
    self.assertEqual(
        _GetTagsToTest(browser),
        set([
            'mac', 'mojave', 'release', 'imagination',
            'imagination-PowerVR-SGX-554', 'opengles', 'passthrough',
            'no-swiftshader-gl', 'no-use-vulkan', 'no-skia-renderer'
        ]))

  def testGenerateVendorTagUsingDeviceString(self):
    platform = fakes.FakePlatform('mac', 'mojave')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        vendor_string='illegal vendor string',
        device_string='ANGLE (Imagination, Triangle Monster 3000, 1.0)')
    self.assertEqual(
        _GetTagsToTest(browser),
        set([
            'mac', 'mojave', 'release', 'imagination',
            'imagination-Triangle-Monster-3000', 'no-angle', 'no-passthrough',
            'no-swiftshader-gl', 'no-use-vulkan', 'no-skia-renderer'
        ]))

  def testSimpleIntegrationTest(self):
    self._RunIntegrationTest(
        test_name='simple_integration_unittest',
        failures=['unexpected_error', 'unexpected_failure'],
        successes=['expected_flaky', 'expected_failure'],
        skips=['expected_skip'],
        additional_args=[
            '--retry-only-retry-on-failure', '--retry-limit=3',
            '--test-name-prefix=unittest_data.integration_tests.SimpleTest.'
        ])
    # The number of browser starts include the one call to StartBrowser at the
    # beginning of the run of the test suite and for each RestartBrowser call
    # which happens after every failure
    self.assertEquals(self._test_state['num_browser_starts'], 6)

  def testIntegrationTesttWithBrowserFailure(self):
    self._RunIntegrationTest(
        test_name='browser_start_failure_integration_unittest',
        failures=[],
        successes=[('unittest_data.integration_tests.'
                    'BrowserStartFailureTest.restart')],
        skips=[], additional_args=[])
    self.assertEquals(self._test_state['num_browser_crashes'], 2)
    self.assertEquals(self._test_state['num_browser_starts'], 3)

  def testIntegrationTestWithBrowserCrashUponStart(self):
    self._RunIntegrationTest(
        test_name='browser_crash_after_start_integration_unittest',
        failures=[],
        successes=[('unittest_data.integration_tests.'
                    'BrowserCrashAfterStartTest.restart')],
        skips=[], additional_args=[])
    self.assertEquals(self._test_state['num_browser_crashes'], 2)
    self.assertEquals(self._test_state['num_browser_starts'], 3)

  def testRetryLimit(self):
    self._RunIntegrationTest(
        test_name='test_retry_limit',
        failures=[('unittest_data.integration_tests'
                   '.TestRetryLimit.unexpected_failure')],
        successes=[], skips=[], additional_args=['--retry-limit=2'])
    # The number of attempted runs is 1 + the retry limit.
    self.assertEquals(self._test_state['num_test_runs'], 3)

  def _RunTestsWithExpectationsFiles(self):
    self._RunIntegrationTest(
        test_name='run_tests_with_expectations_files',
        failures=['a/b/unexpected-fail.html'],
        successes=['a/b/expected-fail.html', 'a/b/expected-flaky.html'],
        skips=['should_skip'],
        additional_args=[
            '--retry-limit=3', '--retry-only-retry-on-failure-tests',
            ('--test-name-prefix=unittest_data.integration_tests.'
             'RunTestsWithExpectationsFiles.')
        ])

  def testTestFilterCommandLineArg(self):
    self._RunIntegrationTest(
        test_name='run_tests_with_expectations_files',
        failures=['a/b/unexpected-fail.html'],
        successes=['a/b/expected-fail.html'],
        skips=['should_skip'],
        additional_args=[
            '--retry-limit=3', '--retry-only-retry-on-failure-tests',
            ('--test-filter=a/b/unexpected-fail.html::a/b/expected-fail.html::'
             'should_skip'),
            ('--test-name-prefix=unittest_data.integration_tests.'
             'RunTestsWithExpectationsFiles.')
        ])

  def testUseTestExpectationsFileToHandleExpectedSkip(self):
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['should_skip']
    self.assertEqual(results['expected'], 'SKIP')
    self.assertEqual(results['actual'], 'SKIP')
    self.assertNotIn('is_regression', results)

  def testUseTestExpectationsFileToHandleUnexpectedTestFailure(self):
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['a']['b']['unexpected-fail.html']
    self.assertEqual(results['expected'], 'PASS')
    self.assertEqual(results['actual'], 'FAIL')
    self.assertIn('is_regression', results)

  def testUseTestExpectationsFileToHandleExpectedFailure(self):
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['a']['b']['expected-fail.html']
    self.assertEqual(results['expected'], 'FAIL')
    self.assertEqual(results['actual'], 'FAIL')
    self.assertNotIn('is_regression', results)

  def testUseTestExpectationsFileToHandleExpectedFlakyTest(self):
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['a']['b']['expected-flaky.html']
    self.assertEqual(results['expected'], 'PASS')
    self.assertEqual(results['actual'], 'FAIL FAIL FAIL PASS')
    self.assertNotIn('is_regression', results)

  def testRepeat(self):
    self._RunIntegrationTest(
        test_name='test_repeat',
        failures=[],
        successes=['unittest_data.integration_tests.TestRepeat.success'],
        skips=[],
        additional_args=['--repeat=3'])
    self.assertEquals(self._test_state['num_test_runs'], 3)

  def testAlsoRunDisabledTests(self):
    self._RunIntegrationTest(
        test_name='test_also_run_disabled_tests',
        failures=['skip', 'flaky'],
        # Tests that are expected to fail and do fail are treated as test passes
        successes=['expected_failure'],
        skips=[],
        additional_args=[
            '--all', '--test-name-prefix',
            'unittest_data.integration_tests.TestAlsoRunDisabledTests.',
            '--retry-limit=3', '--retry-only-retry-on-failure'
        ])
    self.assertEquals(self._test_state['num_flaky_test_runs'], 4)
    self.assertEquals(self._test_state['num_test_runs'], 6)

  def testStartBrowser_Retries(self):
    class TestException(Exception):
      pass

    def SetBrowserAndRaiseTestException():
      gpu_integration_test.GpuIntegrationTest.browser = (mock.MagicMock())
      raise TestException

    gpu_integration_test.GpuIntegrationTest.browser = None
    gpu_integration_test.GpuIntegrationTest.platform = None
    with mock.patch.object(
        gpu_integration_test.serially_executed_browser_test_case.\
            SeriallyExecutedBrowserTestCase,
        'StartBrowser',
        side_effect=SetBrowserAndRaiseTestException) as mock_start_browser:
      with mock.patch.object(gpu_integration_test.GpuIntegrationTest,
                             'StopBrowser') as mock_stop_browser:
        with self.assertRaises(TestException):
          gpu_integration_test.GpuIntegrationTest.StartBrowser()
        self.assertEqual(mock_start_browser.call_count,
                         gpu_integration_test._START_BROWSER_RETRIES)
        self.assertEqual(mock_stop_browser.call_count,
                         gpu_integration_test._START_BROWSER_RETRIES)

  def _RunIntegrationTest(self, test_name, failures, successes, skips,
                          additional_args):
    # pylint: disable=too-many-locals
    config = chromium_config.ChromiumConfig(
        top_level_dir=path_util.GetGpuTestDir(),
        benchmark_dirs=[
            os.path.join(path_util.GetGpuTestDir(), 'unittest_data')
        ])

    with binary_manager.TemporarilyReplaceBinaryManager(None), \
         tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      test_results_path = os.path.join(temp_dir, 'test_results.json')
      test_state_path = os.path.join(temp_dir, 'test_state.json')
      # We are processing ChromiumConfig instance and getting the argument
      # list. Then we pass it directly to run_browser_tests.RunTests. If
      # we called browser_test_runner.Run, then it would spawn another
      # subprocess which is less efficient.
      args = browser_test_runner.ProcessConfig(
          config,
          [
              test_name,
              '--write-full-results-to=%s' % test_results_path,
              '--test-state-json-path=%s' % test_state_path
          ] + additional_args)
      run_browser_tests.RunTests(args)
      with open(test_results_path) as f:
        self._test_result = json.load(f)
      with open(test_state_path) as f:
        self._test_state = json.load(f)
      actual_successes, actual_failures, actual_skips = (_ExtractTestResults(
          self._test_result))
      self.assertEquals(set(actual_failures), set(failures))
      self.assertEquals(set(actual_successes), set(successes))
      self.assertEquals(set(actual_skips), set(skips))


def _ExtractTestResults(test_result):
  delimiter = test_result['path_delimiter']
  failures = []
  successes = []
  skips = []

  def _IsLeafNode(node):
    test_dict = node[1]
    return ('expected' in test_dict
            and isinstance(test_dict['expected'], basestring))

  node_queues = []
  for t in test_result['tests']:
    node_queues.append((t, test_result['tests'][t]))
  while node_queues:
    node = node_queues.pop()
    full_test_name, test_dict = node
    if _IsLeafNode(node):
      if all(res not in test_dict['expected'].split()
             for res in test_dict['actual'].split()):
        failures.append(full_test_name)
      elif test_dict['expected'] == test_dict['actual'] == 'SKIP':
        skips.append(full_test_name)
      else:
        successes.append(full_test_name)
    else:
      for k in test_dict:
        node_queues.append(
            ('%s%s%s' % (full_test_name, delimiter, k), test_dict[k]))
  return successes, failures, skips
