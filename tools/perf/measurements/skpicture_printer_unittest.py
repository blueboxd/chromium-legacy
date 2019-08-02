# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import shutil
import tempfile

from telemetry import decorators
from telemetry.testing import options_for_unittests
from telemetry.testing import page_test_test_case

from measurements import skpicture_printer


class SkpicturePrinterUnitTest(page_test_test_case.PageTestTestCase):

  def setUp(self):
    self._skp_outdir = tempfile.mkdtemp('_skp_test')
    self._options = options_for_unittests.GetRunOptions(
        output_dir=self._skp_outdir)

  def tearDown(self):
    shutil.rmtree(self._skp_outdir)

  # Picture printing is not supported on all platforms.
  @decorators.Disabled('android', 'chromeos')
  def testSkpicturePrinter(self):
    story_set = self.CreateStorySetFromFileInUnittestDataDir('blank.html')
    measurement = skpicture_printer.SkpicturePrinter(self._skp_outdir)
    results = self.RunMeasurement(
        measurement, story_set, run_options=self._options)

    saved_picture_count = results.FindAllPageSpecificValuesNamed(
        'saved_picture_count')
    self.assertEquals(len(saved_picture_count), 1)
    self.assertGreater(saved_picture_count[0].value, 0)
