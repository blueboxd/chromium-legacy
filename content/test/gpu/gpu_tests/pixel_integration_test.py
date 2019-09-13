# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import re
import subprocess
from subprocess import CalledProcessError
import shutil
import sys
import tempfile

from gpu_tests import gpu_integration_test
from gpu_tests import cloud_storage_integration_test_base
from gpu_tests import path_util
from gpu_tests import pixel_test_pages
from gpu_tests import color_profile_manager

from telemetry.util import image_util

gpu_relative_path = "content/test/data/gpu/"
gpu_data_dir = os.path.join(path_util.GetChromiumSrcDir(), gpu_relative_path)

default_reference_image_dir = os.path.join(gpu_data_dir, 'gpu_reference')

test_data_dirs = [gpu_data_dir,
                  os.path.join(
                      path_util.GetChromiumSrcDir(), 'media/test/data')]

test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._proceed = false;

  domAutomationController._readyForActions = false;
  domAutomationController._succeeded = false;
  domAutomationController._finished = false;

  domAutomationController.send = function(msg) {
    domAutomationController._proceed = true;
    let lmsg = msg.toLowerCase();
    if (lmsg == "ready") {
      domAutomationController._readyForActions = true;
    } else {
      domAutomationController._finished = true;
      if (lmsg == "success") {
        domAutomationController._succeeded = true;
      } else {
        domAutomationController._succeeded = false;
      }
    }
  }

  window.domAutomationController = domAutomationController;
"""

goldctl_bin = os.path.join(
    path_util.GetChromiumSrcDir(), 'tools', 'skia_goldctl')
if sys.platform == 'win32':
  goldctl_bin = os.path.join(goldctl_bin, 'win', 'goldctl') + '.exe'
elif sys.platform == 'darwin':
  goldctl_bin = os.path.join(goldctl_bin, 'mac', 'goldctl')
else:
  goldctl_bin = os.path.join(goldctl_bin, 'linux', 'goldctl')

SKIA_GOLD_INSTANCE = 'chrome-gpu'


class _ImageParameters(object):
  def __init__(self):
    # Parameters for cloud storage reference images.
    self.vendor_id = None
    self.device_id = None
    self.vendor_string = None
    self.device_string = None
    self.msaa = False
    self.model_name = None


class PixelIntegrationTest(
    cloud_storage_integration_test_base.CloudStorageIntegrationTestBase):

  test_base_name = 'Pixel'

  # This information is class-scoped, so that it can be shared across
  # invocations of tests; but it's zapped every time the browser is
  # restarted with different command line arguments.
  _image_parameters = None

  _skia_gold_temp_dir = None

  @classmethod
  def Name(cls):
    """The name by which this test is invoked on the command line."""
    return 'pixel'

  @classmethod
  def SetUpProcess(cls):
    options = cls.GetParsedCommandLineOptions()
    color_profile_manager.ForceUntilExitSRGB(
      options.dont_restore_color_profile_after_test)
    super(PixelIntegrationTest, cls).SetUpProcess()
    cls.CustomizeBrowserArgs(cls._AddDefaultArgs([]))
    cls.StartBrowser()
    cls.SetStaticServerDirs(test_data_dirs)
    cls._skia_gold_temp_dir = tempfile.mkdtemp()

  @staticmethod
  def _AddDefaultArgs(browser_args):
    if not browser_args:
      browser_args = []
    # All tests receive the following options.
    return [
      '--force-color-profile=srgb',
      '--ensure-forced-color-profile',
      '--enable-gpu-benchmarking',
      '--test-type=gpu'] + browser_args

  @classmethod
  def StopBrowser(cls):
    super(PixelIntegrationTest, cls).StopBrowser()
    cls.ResetGpuInfo()

  @classmethod
  def TearDownProcess(cls):
    super(PixelIntegrationTest, cls).TearDownProcess()
    if not cls.GetParsedCommandLineOptions().local_run:
      shutil.rmtree(cls._skia_gold_temp_dir)

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(PixelIntegrationTest, cls).AddCommandlineArgs(parser)
    parser.add_option(
      '--reference-dir',
      help='Overrides the default on-disk location for reference images '
      '(only used for local testing without a cloud storage account)',
      default=default_reference_image_dir)
    parser.add_option(
      '--review-patch-issue',
      help='For Skia Gold integration. Gerrit issue ID.',
      default='')
    parser.add_option(
      '--review-patch-set',
      help='For Skia Gold integration. Gerrit patch set number.',
      default='')
    parser.add_option(
      '--buildbucket-build-id',
      help='For Skia Gold integration. Buildbucket build ID.',
      default='')
    parser.add_option(
      '--no-skia-gold-failure',
      action='store_true', default=False,
      help='For Skia Gold integration. Always report that the test passed even '
           'if the Skia Gold image comparison reported a failure, but '
           'otherwise perform the same steps as usual.')
    parser.add_option(
      '--local-run',
      action='store_true', default=False,
      help='Runs the tests in a manner more suitable for local testing. '
           'Specifically, runs goldctl in extra_imgtest_args mode (no upload) '
           'and outputs local links to generated images. Implies '
           '--no-luci-auth.')
    parser.add_option(
      '--no-luci-auth',
      action='store_true', default=False,
      help='Don\'t use the service account provided by LUCI for authentication '
           'for Skia Gold, instead relying on gsutil to be pre-authenticated. '
           'Meant for testing locally instead of on the bots.')

  @classmethod
  def GenerateGpuTests(cls, options):
    cls.SetParsedCommandLineOptions(options)
    namespace = pixel_test_pages.PixelTestPages
    pages = namespace.DefaultPages(cls.test_base_name)
    pages += namespace.GpuRasterizationPages(cls.test_base_name)
    pages += namespace.ExperimentalCanvasFeaturesPages(cls.test_base_name)
    # pages += namespace.NoGpuProcessPages(cls.test_base_name)
    # The following pages should run only on platforms where SwiftShader is
    # enabled. They are skipped on other platforms through test expectations.
    # pages += namespace.SwiftShaderPages(cls.test_base_name)
    if sys.platform.startswith('darwin'):
      pages += namespace.MacSpecificPages(cls.test_base_name)
    if sys.platform.startswith('win'):
      pages += namespace.DirectCompositionPages(cls.test_base_name)
    for p in pages:
      yield(p.name, gpu_relative_path + p.url, (p))

  @classmethod
  def ResetGpuInfo(cls):
    cls._image_parameters = None

  @classmethod
  def GetImageParameters(cls, tab, page):
    if not cls._image_parameters:
      cls._ComputeGpuInfo(tab, page)
    return cls._image_parameters

  @classmethod
  def _ComputeGpuInfo(cls, tab, page):
    if cls._image_parameters:
      return
    browser = cls.browser
    system_info = browser.GetSystemInfo()
    if not system_info:
      raise Exception('System info must be supported by the browser')
    if not system_info.gpu:
      raise Exception('GPU information was absent')
    device = system_info.gpu.devices[0]
    cls._image_parameters = _ImageParameters()
    params = cls._image_parameters
    if device.vendor_id and device.device_id:
      params.vendor_id = device.vendor_id
      params.device_id = device.device_id
    elif device.vendor_string and device.device_string:
      params.vendor_string = device.vendor_string
      params.device_string = device.device_string
    elif page.gpu_process_disabled:
      # Match the vendor and device IDs that the browser advertises
      # when the software renderer is active.
      params.vendor_id = 65535
      params.device_id = 65535
    else:
      raise Exception('GPU device information was incomplete')
    # TODO(senorblanco): This should probably be checking
    # for the presence of the extensions in system_info.gpu_aux_attributes
    # in order to check for MSAA, rather than sniffing the blacklist.
    params.msaa = not (
        ('disable_chromium_framebuffer_multisample' in
          system_info.gpu.driver_bug_workarounds) or
        ('disable_multisample_render_to_texture' in
          system_info.gpu.driver_bug_workarounds))
    params.model_name = system_info.model_name

  # Not used consistently, but potentially useful for debugging issues on the
  # bots, so kept around for future use.
  @classmethod
  def _UploadGoldErrorImageToCloudStorage(cls, image_name, screenshot):
    machine_name = re.sub(r'\W+', '_',
                          cls.GetParsedCommandLineOptions().test_machine_name)
    base_bucket = '%s/gold_failures' % (cls._error_image_cloud_storage_bucket)
    image_name_with_revision_and_machine = '%s_%s_%s.png' % (
      image_name, machine_name,
      cls.GetParsedCommandLineOptions().build_revision)
    cls._UploadBitmapToCloudStorage(
      base_bucket, image_name_with_revision_and_machine, screenshot,
      public=True)

  def ToHex(self, num):
    return hex(int(num))

  def ToHexOrNone(self, num):
    return 'None' if num == None else self.ToHex(num)

  def RunActualGpuTest(self, test_path, *args):
    page = args[0]
    # Some pixel tests require non-standard browser arguments. Need to
    # check before running each page that it can run in the current
    # browser instance.
    self.RestartBrowserIfNecessaryWithArgs(self._AddDefaultArgs(
      page.browser_args))
    url = self.UrlOfStaticFilePath(test_path)
    # This property actually comes off the class, not 'self'.
    tab = self.tab
    tab.Navigate(url, script_to_evaluate_on_commit=test_harness_script)
    tab.action_runner.WaitForJavaScriptCondition(
      'domAutomationController._proceed', timeout=300)
    do_page_action = tab.EvaluateJavaScript(
      'domAutomationController._readyForActions')
    if do_page_action:
      self._DoPageAction(tab, page)
    self._RunSkiaGoldBasedPixelTest(do_page_action, page)

  def _RunSkiaGoldBasedPixelTest(self, do_page_action, page):
    """Captures and compares a test image using Skia Gold.

    Raises an Exception if the comparison fails.

    Args:
      do_page_action: a bool indicating if an action was run on the page.
      page: the GPU PixelTestPage object for the test.
    """
    tab = self.tab
    try:
      # Actually run the test and capture the screenshot.
      if not tab.EvaluateJavaScript('domAutomationController._succeeded'):
        self.fail('page indicated test failure')
      if not tab.screenshot_supported:
        self.fail('Browser does not support screenshot capture')
      screenshot = tab.Screenshot(5)
      if screenshot is None:
        self.fail('Could not capture screenshot')
      dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
      if page.test_rect:
        screenshot = image_util.Crop(
            screenshot, int(page.test_rect[0] * dpr),
            int(page.test_rect[1] * dpr), int(page.test_rect[2] * dpr),
            int(page.test_rect[3] * dpr))

      # Get all the information that goldctl requires.
      parsed_options = self.GetParsedCommandLineOptions()
      build_id_args = [
        '--commit',
        parsed_options.build_revision,
      ]
      # If --review-patch-issue is passed, then we assume we're running on a
      # trybot.
      if parsed_options.review_patch_issue:
        build_id_args += [
          '--issue',
          parsed_options.review_patch_issue,
          '--patchset',
          parsed_options.review_patch_set,
          '--jobid',
          parsed_options.buildbucket_build_id
        ]

      # Compare images against approved images/colors.
      if page.expected_colors:
        # Use expected colors instead of hash comparison for validation.
        self._ValidateScreenshotSamplesWithSkiaGold(
            tab, page, screenshot, dpr, build_id_args)
        return
      image_name = self._UrlToImageName(page.name)
      self._UploadTestResultToSkiaGold(
        image_name, screenshot,
        tab, page,
        build_id_args=build_id_args)
    finally:
      if do_page_action:
        # Assume that page actions might have killed the GPU process.
        self._RestartBrowser('Must restart after page actions')

  def _UploadTestResultToSkiaGold(self, image_name, screenshot,
                                  tab, page, build_id_args=None):
    """Compares the given image using Skia Gold and uploads the result.

    No uploading is done if the test is being run in local run mode. Compares
    the given screenshot to baselines provided by Gold, raising an Exception if
    a match is not found.

    Args:
      image_name: the name of the image being checked.
      screenshot: the image being checked as a Telemetry Bitmap.
      tab: the Telemetry Tab object that the test was run in.
      page: the GPU PixelTestPage object for the test.
      build_id_args: a list of build-identifying flags and values.
    """
    if not isinstance(build_id_args, list) or '--commit' not in build_id_args:
      raise Exception('Requires build args to be specified, including --commit')

    # Write screenshot to PNG file on local disk.
    png_temp_file = tempfile.NamedTemporaryFile(
        suffix='.png', dir=self._skia_gold_temp_dir).name
    image_util.WritePngFile(screenshot, png_temp_file)

    # Get all information that goldctl will need.
    img_params = self.GetImageParameters(tab, page)
    # All values need to be strings, otherwise goldctl fails.
    gpu_keys = {
      'vendor_id': self.ToHexOrNone(img_params.vendor_id),
      'device_id': self.ToHexOrNone(img_params.device_id),
      'vendor_string': str(img_params.vendor_string),
      'device_string': str(img_params.device_string),
      'msaa': str(img_params.msaa),
      'model_name': str(img_params.model_name),
    }
    json_temp_file = tempfile.NamedTemporaryFile(
        suffix='.json', dir=self._skia_gold_temp_dir).name
    failure_file = tempfile.NamedTemporaryFile(
        suffix='.txt', dir=self._skia_gold_temp_dir).name
    with open(json_temp_file, 'w+') as f:
      json.dump(gpu_keys, f)

    # Figure out any extra args we need to pass to goldctl.
    extra_imgtest_args = []
    extra_auth_args = []
    parsed_options = self.GetParsedCommandLineOptions()
    if parsed_options.local_run:
      extra_imgtest_args.append('--dryrun')
    elif not parsed_options.no_luci_auth:
      extra_auth_args = ['--luci']

    # Run goldctl for a result.
    try:
      subprocess.check_output([goldctl_bin, 'auth',
                               '--work-dir', self._skia_gold_temp_dir]
                               + extra_auth_args,
            stderr=subprocess.STDOUT)
      cmd = ([goldctl_bin, 'imgtest', 'add', '--passfail',
              '--test-name', image_name,
              '--instance', SKIA_GOLD_INSTANCE,
              '--keys-file', json_temp_file,
              '--png-file', png_temp_file,
              '--work-dir', self._skia_gold_temp_dir,
              '--failure-file', failure_file] +
              build_id_args + extra_imgtest_args)
      subprocess.check_output(cmd, stderr=subprocess.STDOUT)
    except CalledProcessError as e:
      try:
        # The triage link for the image is output to the failure file, so report
        # that if it's available so it shows up in Milo. If for whatever reason
        # the file is not present or malformed, the triage link will still be
        # present in the stdout of the goldctl command.
        with open(failure_file, 'r') as ff:
          self.artifacts.CreateLink('gold_triage_link', ff.read())
      except Exception:
        logging.error('Failed to read contents of goldctl failure file')
      logging.error('goldctl failed with output: %s', e.output)
      if parsed_options.local_run:
        logging.error(
            'Image produced by %s: file://%s', image_name, png_temp_file)
        gold_images = ('https://%s-gold.skia.org/search?'
                      'match=name&metric=combined&pos=true&'
                      'query=name%%3D%s&unt=false' % (
                          SKIA_GOLD_INSTANCE, image_name))
        logging.error(
            'Approved images for %s in Gold: %s', image_name, gold_images)
      if not parsed_options.no_skia_gold_failure:
        raise Exception('goldctl command failed')

  def _ValidateScreenshotSamplesWithSkiaGold(self, tab, page, screenshot,
                                             device_pixel_ratio,
                                             build_id_args):
    """Samples the given screenshot and verifies pixel color values.

    In case any of the samples do not match the expected color, it raises
    a Failure and uploads the image to Gold.

    Args:
      tab: the Telemetry Tab object that the test was run in.
      page: the GPU PixelTestPage object for the test.
      screenshot: the screenshot of the test page as a Telemetry Bitmap.
      device_pixel_ratio: the device pixel ratio for the test device as a float.
      build_id_args: a list of build-identifying flags and values.
    """
    try:
      self._CompareScreenshotSamples(
        tab, screenshot, page.expected_colors, page.tolerance,
        device_pixel_ratio,
        self.GetParsedCommandLineOptions().test_machine_name)
    except Exception:
      # An exception raised from self.fail() indicates a failure.
      image_name = self._UrlToImageName(page.name)
      # We want to report the screenshot comparison failure, not any failures
      # related to Gold.
      try:
        self._UploadTestResultToSkiaGold(
          image_name, screenshot,
          tab, page,
          build_id_args=build_id_args)
      except Exception as e:
        logging.error(str(e))
      raise

  def _DoPageAction(self, tab, page):
    getattr(self, '_' + page.optional_action)(tab, page)
    # Now that we've done the page's specific action, wait for it to
    # report completion.
    tab.action_runner.WaitForJavaScriptCondition(
      'domAutomationController._finished', timeout=300)

  #
  # Optional actions pages can take.
  # These are specified as methods taking the tab and the page as
  # arguments.
  #
  def _CrashGpuProcess(self, tab, page):
    # Crash the GPU process.
    #
    # This used to create a new tab and navigate it to
    # chrome://gpucrash, but there was enough unreliability
    # navigating between these tabs (one of which was created solely
    # in order to navigate to chrome://gpucrash) that the simpler
    # solution of provoking the GPU process crash from this renderer
    # process was chosen.
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')

  def _SwitchTabs(self, tab, page):
    if not tab.browser.supports_tab_control:
      self.fail('Browser must support tab control')
    dummy_tab = tab.browser.tabs.New()
    dummy_tab.Activate()
    # Wait for 2 seconds so that new tab becomes visible.
    dummy_tab.action_runner.Wait(2)
    tab.Activate()

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations',
                     'pixel_expectations.txt')]

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
