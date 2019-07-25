# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base classes for a test which uploads results (reference images,
error images) to cloud storage."""

import logging
import os
import re

import tempfile

from py_utils import cloud_storage
from telemetry.util import image_util
from telemetry.util import rgba_color

from gpu_tests import gpu_integration_test

test_data_dir = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', 'data', 'gpu'))

default_generated_data_dir = os.path.join(test_data_dir, 'generated')


class CloudStorageIntegrationTestBase(gpu_integration_test.GpuIntegrationTest):
  # This class is abstract; don't warn about the superclass's abstract
  # methods that aren't overridden.
  # pylint: disable=abstract-method

  # This information is class-scoped, so that it can be shared across
  # invocations of tests; but it's zapped every time the browser is
  # restarted with different command line arguments.
  _reference_image_parameters = None

  # The command line options (which are passed to subclasses'
  # GenerateGpuTests) *must* be configured here, via a call to
  # SetParsedCommandLineOptions. If they are not, an error will be
  # raised when running the tests.
  _parsed_command_line_options = None

  _skia_gold_temp_dir = None

  _error_image_cloud_storage_bucket = 'chromium-browser-gpu-tests'

  @classmethod
  def SetParsedCommandLineOptions(cls, options):
    cls._parsed_command_line_options = options

  @classmethod
  def GetParsedCommandLineOptions(cls):
    return cls._parsed_command_line_options

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(CloudStorageIntegrationTestBase, cls).AddCommandlineArgs(parser)
    parser.add_option(
      '--build-revision',
      help='Chrome revision being tested.',
      default="unknownrev")
    parser.add_option(
      '--os-type',
      help='Type of operating system on which the pixel test is being run, '
      'used only to distinguish different operating systems with the same '
      'graphics card. Any value is acceptable, but canonical values are '
      '"win", "mac", and "linux", and probably, eventually, "chromeos" '
      'and "android").',
      default='')
    parser.add_option(
      '--test-machine-name',
      help='Name of the test machine. Specifying this argument causes this '
      'script to upload failure images and diffs to cloud storage directly, '
      'instead of relying on the archive_gpu_pixel_test_results.py script.',
      default='')
    parser.add_option(
      '--generated-dir',
      help='Overrides the default on-disk location for generated test images '
      '(only used for local testing without a cloud storage account)',
      default=default_generated_data_dir)
    parser.add_option(
      '--dont-restore-color-profile-after-test',
      dest='dont_restore_color_profile_after_test',
      action='store_true', default=False,
      help='(Mainly on Mac) don\'t restore the system\'s original color '
      'profile after the test completes; leave the system using the sRGB color '
      'profile. See http://crbug.com/784456.')

  def _CompareScreenshotSamples(self, tab, screenshot, expected_colors,
                                tolerance, device_pixel_ratio,
                                test_machine_name):
    # First scan through the expected_colors and see if there are any scale
    # factor overrides that would preempt the device pixel ratio. This
    # is mainly a workaround for complex tests like the Maps test.
    for expectation in expected_colors:
      if 'scale_factor_overrides' in expectation:
        for override in expectation['scale_factor_overrides']:
          # Require exact matches to avoid confusion, because some
          # machine models and names might be subsets of others
          # (e.g. Nexus 5 vs Nexus 5X).
          if ('device_type' in override and
              (tab.browser.platform.GetDeviceTypeName() ==
               override['device_type'])):
            logging.warning(
              'Overriding device_pixel_ratio ' + str(device_pixel_ratio) +
              ' with scale factor ' + str(override['scale_factor']) +
              ' for device type ' + override['device_type'])
            device_pixel_ratio = override['scale_factor']
            break
          if (test_machine_name and 'machine_name' in override and
              override["machine_name"] == test_machine_name):
            logging.warning(
              'Overriding device_pixel_ratio ' + str(device_pixel_ratio) +
              ' with scale factor ' + str(override['scale_factor']) +
              ' for machine name ' + test_machine_name)
            device_pixel_ratio = override['scale_factor']
            break
        # Only support one "scale_factor_overrides" in the expectation format.
        break
    for expectation in expected_colors:
      if "scale_factor_overrides" in expectation:
        continue
      location = expectation["location"]
      size = expectation["size"]
      x0 = int(location[0] * device_pixel_ratio)
      x1 = int((location[0] + size[0]) * device_pixel_ratio)
      y0 = int(location[1] * device_pixel_ratio)
      y1 = int((location[1] + size[1]) * device_pixel_ratio)
      for x in range(x0, x1):
        for y in range(y0, y1):
          if (x < 0 or y < 0 or x >= image_util.Width(screenshot) or
              y >= image_util.Height(screenshot)):
            self.fail(
                ('Expected pixel location [%d, %d] is out of range on ' +
                 '[%d, %d] image') %
                (x, y, image_util.Width(screenshot),
                 image_util.Height(screenshot)))

          actual_color = image_util.GetPixelColor(screenshot, x, y)
          expected_color = rgba_color.RgbaColor(
              expectation["color"][0],
              expectation["color"][1],
              expectation["color"][2])
          if not actual_color.IsEqual(expected_color, tolerance):
            self.fail('Expected pixel at ' + str(location) +
                ' (actual pixel (' + str(x) + ', ' + str(y) + ')) ' +
                ' to be ' +
                str(expectation["color"]) + " but got [" +
                str(actual_color.r) + ", " +
                str(actual_color.g) + ", " +
                str(actual_color.b) + "]")

  ###
  ### Routines working with the local disk (only used for local
  ### testing without a cloud storage account -- the bots do not use
  ### this code path).
  ###

  def _UrlToImageName(self, url):
    image_name = re.sub(r'^(http|https|file)://(/*)', '', url)
    image_name = re.sub(r'\.\./', '', image_name)
    image_name = re.sub(r'(\.|/|-)', '_', image_name)
    return image_name

  def _WriteImage(self, image_path, png_image):
    output_dir = os.path.dirname(image_path)
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    image_util.WritePngFile(png_image, image_path)

  def _WriteErrorImages(self, img_dir, img_name, screenshot, ref_png):
    full_image_name = img_name + '_' + str(
      self.GetParsedCommandLineOptions().build_revision)
    full_image_name = full_image_name + '.png'

    # Always write the failing image.
    self._WriteImage(
        os.path.join(img_dir, 'FAIL_' + full_image_name), screenshot)

    if ref_png is not None:
      # Save the reference image.
      # This ensures that we get the right revision number.
      self._WriteImage(
          os.path.join(img_dir, full_image_name), ref_png)

      # Save the difference image.
      diff_png = image_util.Diff(screenshot, ref_png)
      self._WriteImage(
          os.path.join(img_dir, 'DIFF_' + full_image_name), diff_png)

  @classmethod
  def _UploadBitmapToCloudStorage(cls, bucket, name, bitmap, public=False):
    # This sequence of steps works on all platforms to write a temporary
    # PNG to disk, following the pattern in bitmap_unittest.py. The key to
    # avoiding PermissionErrors seems to be to not actually try to write to
    # the temporary file object, but to re-open its name for all operations.
    temp_file = tempfile.NamedTemporaryFile(suffix='.png').name
    image_util.WritePngFile(bitmap, temp_file)
    cloud_storage.Insert(bucket, name, temp_file, publicly_readable=public)

  @classmethod
  def GenerateGpuTests(cls, options):
    del options
    return []
