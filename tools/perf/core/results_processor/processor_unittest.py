# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for results_processor methods."""

import unittest

from core.results_processor import processor
from core.results_processor import testing

from tracing.value import histogram
from tracing.value import histogram_set


class ResultsProcessorUnitTests(unittest.TestCase):
  def testAddDiagnosticsToHistograms(self):
    histogram_dicts = [histogram.Histogram('a', 'unitless').AsDict()]

    in_results = testing.IntermediateResults(
        test_results=[],
        diagnostics={
            'benchmarks': ['benchmark'],
            'osNames': ['linux'],
            'documentationUrls': [['documentation', 'url']],
        },
    )

    histograms_with_diagnostics = processor.AddDiagnosticsToHistograms(
        histogram_dicts, in_results, results_label='label')

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(histograms_with_diagnostics)
    diag_values = [list(v) for v in  out_histograms.shared_diagnostics]
    self.assertEqual(len(diag_values), 4)
    self.assertIn(['benchmark'], diag_values)
    self.assertIn(['linux'], diag_values)
    self.assertIn([['documentation', 'url']], diag_values)
    self.assertIn(['label'], diag_values)
