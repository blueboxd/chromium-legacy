# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import os
import sys

from gpu_tests import color_profile_manager
from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import pixel_test_pages
from gpu_tests import skia_gold_integration_test_base

from py_utils import cloud_storage

_MAPS_PERF_TEST_PATH = os.path.join(path_util.GetChromiumSrcDir(), 'tools',
                                    'perf', 'page_sets', 'maps_perf_test')

_DATA_PATH = os.path.join(path_util.GetChromiumSrcDir(), 'content', 'test',
                          'gpu', 'gpu_tests')

_TEST_NAME = 'Maps_maps'


class MapsIntegrationTest(
    skia_gold_integration_test_base.SkiaGoldIntegrationTestBase):
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
        '--force-color-profile=srgb', '--ensure-forced-color-profile'
    ]
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
    # The maps_pixel_expectations.json contain the actual image expectations. If
    # the test fails, with errors greater than the tolerance for the run, then
    # the logs will report the actual failure.
    #
    # There will also be a Skia Gold Triage link, this will be used to store the
    # artifact of the failure to help with debugging. There are no accepted
    # positive baselines recorded in Skia Gold, so its diff will not be
    # sufficient to debugging the failure.
    yield ('Maps_maps', 'file://performance.html', ())

  def RunActualGpuTest(self, url, *_):
    tab = self.tab
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

    screenshot = tab.Screenshot(5)
    if screenshot is None:
      self.fail('Could not capture screenshot')

    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    print 'Maps\' devicePixelRatio is ' + str(dpr)
    page = _GetMapsPageForUrl(url)
    self._UploadTestResultToSkiaGold(_TEST_NAME, screenshot, page,
                                     self._GetBuildIdArgs())

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'maps_expectations.txt')
    ]


def _GetMapsPageForUrl(url):
  page = pixel_test_pages.PixelTestPage(
      url=url,
      name=_TEST_NAME,
      # Exact test_rect is arbitrary, just needs to encapsulate all pixels
      # that are tested.
      test_rect=[0, 0, 600, 400],
      grace_period_end=datetime.date(2020, 6, 22),
      matching_algorithm=pixel_test_pages.VERY_PERMISSIVE_SOBEL_ALGO)
  return page


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
