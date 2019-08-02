# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import shutil
import tempfile

from telemetry import decorators
from telemetry.testing import options_for_unittests
from telemetry.testing import page_test_test_case

from measurements import multipage_skpicture_printer


class MultipageSkpicturePrinterUnitTest(page_test_test_case.PageTestTestCase):

  def setUp(self):
    self._mskp_outdir = tempfile.mkdtemp('_mskp_test')
    self._options = options_for_unittests.GetRunOptions(
        output_dir=self._mskp_outdir)

  def tearDown(self):
    shutil.rmtree(self._mskp_outdir)

  # Picture printing is not supported on all platforms.
  @decorators.Disabled('android', 'chromeos')
  def testSkpicturePrinter(self):
    story_set = self.CreateStorySetFromFileInUnittestDataDir('blank.html')
    measurement = multipage_skpicture_printer.MultipageSkpicturePrinter(
        self._mskp_outdir)
    self.RunMeasurement(measurement, story_set, run_options=self._options)
