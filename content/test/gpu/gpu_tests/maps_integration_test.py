# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import sys

from gpu_tests import gpu_integration_test
from gpu_tests import cloud_storage_integration_test_base
from gpu_tests import path_util
from gpu_tests import color_profile_manager

from py_utils import cloud_storage

from telemetry.util import image_util

_MAPS_PERF_TEST_PATH = os.path.join(
  path_util.GetChromiumSrcDir(), 'tools', 'perf', 'page_sets', 'maps_perf_test')

_DATA_PATH = os.path.join(path_util.GetChromiumSrcDir(),
                         'content', 'test', 'gpu', 'gpu_tests')

_TOLERANCE = 3

class MapsIntegrationTest(
    cloud_storage_integration_test_base.CloudStorageIntegrationTestBase):
  """Google Maps pixel tests.

  Note: this test uses the same WPR as the smoothness.maps benchmark
  in tools/perf/benchmarks. See src/tools/perf/page_sets/maps.py for
  documentation on updating the WPR archive.
  """

  @classmethod
  def Name(cls):
    return 'maps'

  @classmethod
  def SetUpProcess(cls):
    options = cls.GetParsedCommandLineOptions()
    color_profile_manager.ForceUntilExitSRGB(
      options.dont_restore_color_profile_after_test)
    super(MapsIntegrationTest, cls).SetUpProcess()
    browser_args = [
        '--force-color-profile=srgb',
        '--ensure-forced-color-profile']
    cls.CustomizeBrowserArgs(browser_args)
    cloud_storage.GetIfChanged(
      os.path.join(_MAPS_PERF_TEST_PATH, 'load_dataset'),
      cloud_storage.PUBLIC_BUCKET)
    cls.SetStaticServerDirs([_MAPS_PERF_TEST_PATH])
    cls.StartBrowser()

  @classmethod
  def TearDownProcess(cls):
    super(cls, MapsIntegrationTest).TearDownProcess()
    cls.StopWPRServer()

  @classmethod
  def GenerateGpuTests(cls, options):
    cls.SetParsedCommandLineOptions(options)
    yield('Maps_maps',
          'file://performance.html',
          ('maps_pixel_expectations.json'))

  def _ReadPixelExpectations(self, expectations_file):
    expectations_path = os.path.join(_DATA_PATH, expectations_file)
    with open(expectations_path, 'r') as f:
      json_contents = json.load(f)
    return json_contents

  @classmethod
  def _UploadErrorImagesToCloudStorage(cls, image_name, screenshot, ref_img):
    """For a failing run, uploads the failing image, reference image (if
    supplied), and diff image (if reference image was supplied) to cloud
    storage. This subsumes the functionality of the
    archive_gpu_pixel_test_results.py script."""
    machine_name = re.sub(r'\W+', '_',
                          cls.GetParsedCommandLineOptions().test_machine_name)
    upload_dir = '%s_%s_telemetry' % (
      cls.GetParsedCommandLineOptions().build_revision, machine_name)
    base_bucket = '%s/runs/%s' % (
        cls._error_image_cloud_storage_bucket, upload_dir)
    image_name_with_revision = '%s_%s.png' % (
      image_name, cls.GetParsedCommandLineOptions().build_revision)
    cls._UploadBitmapToCloudStorage(
      base_bucket + '/gen', image_name_with_revision, screenshot,
      public=True)
    if ref_img is not None:
      cls._UploadBitmapToCloudStorage(
        base_bucket + '/ref', image_name_with_revision, ref_img, public=True)
      diff_img = image_util.Diff(screenshot, ref_img)
      cls._UploadBitmapToCloudStorage(
        base_bucket + '/diff', image_name_with_revision, diff_img,
        public=True)
    print ('See http://%s.commondatastorage.googleapis.com/'
           'view_test_results.html?%s for this run\'s test results') % (
      cls._error_image_cloud_storage_bucket, upload_dir)

  def _ValidateScreenshotSamples(self, tab, url, screenshot, expectations,
                                 tolerance, device_pixel_ratio):
    """Samples the given screenshot and verifies pixel color values.
       The sample locations and expected color values are given in expectations.
       In case any of the samples do not match the expected color, it raises
       a Failure and dumps the screenshot locally or cloud storage depending on
       what machine the test is being run."""
    try:
      self._CompareScreenshotSamples(
        tab, screenshot, expectations, tolerance, device_pixel_ratio,
        self.GetParsedCommandLineOptions().test_machine_name)
    except Exception:
      # An exception raised from self.fail() indicates a failure.
      image_name = self._UrlToImageName(url)
      if self.GetParsedCommandLineOptions().test_machine_name:
        self._UploadErrorImagesToCloudStorage(image_name, screenshot, None)
      else:
        self._WriteErrorImages(
          self.GetParsedCommandLineOptions().generated_dir, image_name,
          screenshot, None)
      raise

  def RunActualGpuTest(self, url, *args):
    tab = self.tab
    pixel_expectations_file = args[0]
    action_runner = tab.action_runner
    action_runner.Navigate(url)
    action_runner.WaitForJavaScriptCondition('window.startTest != undefined')
    action_runner.EvaluateJavaScript('window.startTest()')
    action_runner.WaitForJavaScriptCondition('window.testDone', timeout=320)

    # Wait for the page to process immediate work and load tiles.
    action_runner.EvaluateJavaScript('''
        window.testCompleted = false;
        requestIdleCallback(
            () => window.testCompleted = true,
            { timeout : 10000 })''')
    action_runner.WaitForJavaScriptCondition('window.testCompleted', timeout=30)

    if not tab.screenshot_supported:
      self.fail('Browser does not support screenshot capture')
    screenshot = tab.Screenshot(5)
    if screenshot is None:
      self.fail('Could not capture screenshot')

    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    print 'Maps\' devicePixelRatio is ' + str(dpr)
    # Even though the Maps test uses a fixed devicePixelRatio so that
    # it fetches all of the map tiles at the same resolution, on two
    # different devices with the same devicePixelRatio (a Retina
    # MacBook Pro and a Nexus 9), different scale factors of the final
    # screenshot are observed. Hack around this by specifying a scale
    # factor for these bots in the test expectations. This relies on
    # the test-machine-name argument being specified on the command
    # line.
    expected = self._ReadPixelExpectations(pixel_expectations_file)
    self._ValidateScreenshotSamples(
      tab, url, screenshot, expected, _TOLERANCE, dpr)

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
