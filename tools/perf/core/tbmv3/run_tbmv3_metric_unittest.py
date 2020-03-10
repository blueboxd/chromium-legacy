# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Integration tests for run_tbmv3_metric.py.

These test check run_tbmv3_metric works end to end, using the real trace
processor shell.
"""

import json
import os
import shutil
import tempfile
import unittest

from core.tbmv3 import run_tbmv3_metric

from tracing.value import histogram_set

# For testing the TBMv3 workflow we use dummy_metric defined in
# tools/perf/core/tbmv3/metrics/dummy_metric_*.
# This metric ignores the trace data and outputs a histogram with
# the following name and unit:
DUMMY_HISTOGRAM_NAME = 'dummy::simple_field'
DUMMY_HISTOGRAM_UNIT = 'count_smallerIsBetter'


class RunTbmv3MetricIntegrationTests(unittest.TestCase):
  def setUp(self):
    self.output_dir = tempfile.mkdtemp()
    self.trace_path = self.CreateEmptyProtoTrace()
    self.outfile_path = os.path.join(self.output_dir, 'out.json')

  def tearDown(self):
    shutil.rmtree(self.output_dir)

  def CreateEmptyProtoTrace(self):
    """Create an empty file as a proto trace."""
    with tempfile.NamedTemporaryFile(
        dir=self.output_dir, delete=False) as trace_file:
      # Open temp file and close it so it's written to disk.
      pass
    return trace_file.name

  def testRunTbmv3MetricOnDummyMetric(self):
    run_tbmv3_metric.Main([
        '--trace', self.trace_path,
        '--metric', 'dummy_metric',
        '--outfile', self.outfile_path,
    ])

    with open(self.outfile_path) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    hist = out_histograms.GetHistogramNamed(DUMMY_HISTOGRAM_NAME)
    self.assertEqual(hist.unit, DUMMY_HISTOGRAM_UNIT)
    self.assertEqual(hist.num_values, 1)
    self.assertEqual(hist.average, 42)
