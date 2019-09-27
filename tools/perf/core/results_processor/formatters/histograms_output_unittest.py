# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import json
import os
import shutil
import tempfile

from core.results_processor.formatters import histograms_output
from core.results_processor import command_line
from core.results_processor import testing

from tracing.value import histogram
from tracing.value import histogram_set


class HistogramsOutputTest(unittest.TestCase):
  def setUp(self):
    self.output_dir = tempfile.mkdtemp()
    parser = command_line.ArgumentParser()
    self.options = parser.parse_args([])
    self.options.output_dir = self.output_dir
    command_line.ProcessOptions(self.options)

  def tearDown(self):
    shutil.rmtree(self.output_dir)

  def testConvertOneStory(self):
    hist_file = os.path.join(self.output_dir,
                             histograms_output.HISTOGRAM_DICTS_NAME)
    with open(hist_file, 'w') as f:
      json.dump([histogram.Histogram('a', 'unitless').AsDict()], f)

    in_results = testing.IntermediateResults(
        test_results=[
            testing.TestResult(
                'benchmark/story',
                artifacts={'histogram_dicts.json': testing.Artifact(hist_file)},
            ),
        ],
    )

    histogram_dicts = histograms_output.Convert(in_results, results_label=None)
    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(histogram_dicts)
    self.assertEqual(len(out_histograms), 1)
    self.assertEqual(out_histograms.GetFirstHistogram().name, 'a')
    self.assertEqual(out_histograms.GetFirstHistogram().unit, 'unitless')

  def testConvertDiagnostics(self):
    hist_file = os.path.join(self.output_dir,
                             histograms_output.HISTOGRAM_DICTS_NAME)
    with open(hist_file, 'w') as f:
      json.dump([histogram.Histogram('a', 'unitless').AsDict()], f)

    in_results = testing.IntermediateResults(
        test_results=[],
        diagnostics={
            'benchmarks': ['benchmark'],
            'osNames': ['linux'],
            'documentationUrls': [['documentation', 'url']],
        },
    )

    histogram_dicts = histograms_output.Convert(in_results,
                                                results_label='label')
    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(histogram_dicts)
    diag_values = [list(v) for v in  out_histograms.shared_diagnostics]
    self.assertEqual(len(diag_values), 4)
    self.assertIn(['benchmark'], diag_values)
    self.assertIn(['linux'], diag_values)
    self.assertIn([['documentation', 'url']], diag_values)
    self.assertIn(['label'], diag_values)

  def testConvertTwoStories(self):
    hist_file = os.path.join(self.output_dir,
                             histograms_output.HISTOGRAM_DICTS_NAME)
    with open(hist_file, 'w') as f:
      json.dump([histogram.Histogram('a', 'unitless').AsDict()], f)

    in_results = testing.IntermediateResults(
        test_results=[
            testing.TestResult(
                'benchmark/story1',
                artifacts={'histogram_dicts.json': testing.Artifact(hist_file)},
            ),
            testing.TestResult(
                'benchmark/story2',
                artifacts={'histogram_dicts.json': testing.Artifact(hist_file)},
            ),
            testing.TestResult(
                'benchmark/story1',
                artifacts={'histogram_dicts.json': testing.Artifact(hist_file)},
            ),
            testing.TestResult(
                'benchmark/story2',
                artifacts={'histogram_dicts.json': testing.Artifact(hist_file)},
            ),
        ],
    )

    histogram_dicts = histograms_output.Convert(in_results,
                                                results_label='label')
    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(histogram_dicts)
    self.assertEqual(len(out_histograms), 4)
    hist = out_histograms.GetFirstHistogram()
    self.assertEqual(hist.name, 'a')
    self.assertEqual(hist.unit, 'unitless')
    self.assertEqual(list(hist.diagnostics['labels']), ['label'])
