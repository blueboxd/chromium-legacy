# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Integration tests for results_processor.

These tests write actual files with intermediate results, and run the
standalone results processor on them to check that output files are produced
with their expected contents.
"""

import json
import os
import shutil
import tempfile
import unittest

from core.results_processor.formatters import json3_output
from core.results_processor.formatters import histograms_output
from core.results_processor.formatters import html_output
from core.results_processor import processor
from core.results_processor import testing

from tracing.value import histogram
from tracing.value import histogram_set
from tracing_build import render_histograms_viewer


class ResultsProcessorIntegrationTests(unittest.TestCase):
  def setUp(self):
    self.output_dir = tempfile.mkdtemp()
    self.intermediate_dir = os.path.join(
        self.output_dir, 'artifacts', 'test_run')
    os.makedirs(self.intermediate_dir)

  def tearDown(self):
    shutil.rmtree(self.output_dir)

  def SerializeIntermediateResults(self, *args, **kwargs):
    in_results = testing.IntermediateResults(*args, **kwargs)
    testing.SerializeIntermediateResults(in_results, os.path.join(
        self.intermediate_dir, processor.TELEMETRY_RESULTS))

  def testJson3Output(self):
    self.SerializeIntermediateResults([
        testing.TestResult(
            'benchmark/story', run_duration='1.1s', tags=['shard:7']),
        testing.TestResult(
            'benchmark/story', run_duration='1.2s', tags=['shard:7']),
    ], start_time='2009-02-13T23:31:30.987000Z')

    processor.main([
        '--output-format', 'json-test-results',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir])

    with open(os.path.join(
        self.output_dir, json3_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    self.assertFalse(results['interrupted'])
    self.assertEqual(results['num_failures_by_type'], {'PASS': 2})
    self.assertEqual(results['seconds_since_epoch'], 1234567890.987)
    self.assertEqual(results['version'], 3)

    self.assertIn('benchmark', results['tests'])
    self.assertIn('story', results['tests']['benchmark'])
    test_result = results['tests']['benchmark']['story']

    self.assertEqual(test_result['actual'], 'PASS')
    self.assertEqual(test_result['expected'], 'PASS')
    self.assertEqual(test_result['times'], [1.1, 1.2])
    self.assertEqual(test_result['time'], 1.1)
    self.assertEqual(test_result['shard'], 7)

  def testJson3OutputWithArtifacts(self):
    self.SerializeIntermediateResults([
        testing.TestResult(
            'benchmark/story',
            artifacts={
                'logs': testing.Artifact('/logs.txt', 'gs://logs.txt'),
                'trace/telemetry': testing.Artifact('/telemetry.json'),
                'trace.html':
                    testing.Artifact('/trace.html', 'gs://trace.html'),
            },
    )])

    processor.main([
        '--output-format', 'json-test-results',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir])

    with open(os.path.join(
        self.output_dir, json3_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    self.assertIn('benchmark', results['tests'])
    self.assertIn('story', results['tests']['benchmark'])
    self.assertIn('artifacts', results['tests']['benchmark']['story'])
    artifacts = results['tests']['benchmark']['story']['artifacts']

    self.assertEqual(len(artifacts), 2)
    self.assertEqual(artifacts['logs'], ['gs://logs.txt'])
    self.assertEqual(artifacts['trace.html'], ['gs://trace.html'])

  def testHistogramsOutput(self):
    hist_file = os.path.join(self.output_dir,
                             histograms_output.HISTOGRAM_DICTS_NAME)
    with open(hist_file, 'w') as f:
      json.dump([histogram.Histogram('a', 'unitless').AsDict()], f)

    self.SerializeIntermediateResults(
        test_results=[
            testing.TestResult(
                'benchmark/story',
                artifacts={'histogram_dicts.json': testing.Artifact(hist_file)},
            ),
        ],
        diagnostics={
            'benchmarks': ['benchmark'],
            'osNames': ['linux'],
            'documentationUrls': [['documentation', 'url']],
        },
        start_time='2009-02-13T23:31:30.987000Z',
    )

    processor.main([
        '--output-format', 'histograms',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label',
    ])

    with open(os.path.join(
        self.output_dir, histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)
    self.assertEqual(len(out_histograms), 1)
    self.assertEqual(out_histograms.GetFirstHistogram().name, 'a')
    self.assertEqual(out_histograms.GetFirstHistogram().unit, 'unitless')

    diag_values = [list(v) for v in  out_histograms.shared_diagnostics]
    self.assertEqual(len(diag_values), 4)
    self.assertIn(['benchmark'], diag_values)
    self.assertIn(['linux'], diag_values)
    self.assertIn([['documentation', 'url']], diag_values)
    self.assertIn(['label'], diag_values)

  def testHistogramsOutputResetResults(self):
    hist_file = os.path.join(self.output_dir,
                             histograms_output.HISTOGRAM_DICTS_NAME)
    with open(hist_file, 'w') as f:
      json.dump([histogram.Histogram('a', 'unitless').AsDict()], f)

    self.SerializeIntermediateResults(
        test_results=[
            testing.TestResult(
                'benchmark/story',
                artifacts={'histogram_dicts.json': testing.Artifact(hist_file)},
            ),
        ],
    )

    processor.main([
        '--output-format', 'histograms',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label1',
    ])

    processor.main([
        '--output-format', 'histograms',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label2',
        '--reset-results',
    ])

    with open(os.path.join(
        self.output_dir, histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)
    self.assertEqual(len(out_histograms), 1)
    diag_values = [list(v) for v in  out_histograms.shared_diagnostics]
    self.assertNotIn(['label1'], diag_values)
    self.assertIn(['label2'], diag_values)

  def testHistogramsOutputAppendResults(self):
    hist_file = os.path.join(self.output_dir,
                             histograms_output.HISTOGRAM_DICTS_NAME)
    with open(hist_file, 'w') as f:
      json.dump([histogram.Histogram('a', 'unitless').AsDict()], f)

    self.SerializeIntermediateResults(
        test_results=[
            testing.TestResult(
                'benchmark/story',
                artifacts={'histogram_dicts.json': testing.Artifact(hist_file)},
            ),
        ],
    )

    processor.main([
        '--output-format', 'histograms',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label1',
    ])

    processor.main([
        '--output-format', 'histograms',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label2',
    ])

    with open(os.path.join(
        self.output_dir, histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)
    self.assertEqual(len(out_histograms), 2)
    diag_values = [list(v) for v in  out_histograms.shared_diagnostics]
    self.assertIn(['label1'], diag_values)
    self.assertIn(['label2'], diag_values)

  def testHtmlOutput(self):
    hist_file = os.path.join(self.output_dir,
                             histograms_output.HISTOGRAM_DICTS_NAME)
    with open(hist_file, 'w') as f:
      json.dump([histogram.Histogram('a', 'unitless').AsDict()], f)

    self.SerializeIntermediateResults(
        test_results=[
            testing.TestResult(
                'benchmark/story',
                artifacts={'histogram_dicts.json': testing.Artifact(hist_file)},
            ),
        ],
        diagnostics={
            'benchmarks': ['benchmark'],
            'osNames': ['linux'],
            'documentationUrls': [['documentation', 'url']],
        },
        start_time='2009-02-13T23:31:30.987000Z',
    )

    processor.main([
        '--output-format', 'html',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label',
    ])

    with open(os.path.join(
        self.output_dir, html_output.OUTPUT_FILENAME)) as f:
      results = render_histograms_viewer.ReadExistingResults(f.read())

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)
    self.assertEqual(len(out_histograms), 1)
    self.assertEqual(out_histograms.GetFirstHistogram().name, 'a')
    self.assertEqual(out_histograms.GetFirstHistogram().unit, 'unitless')

    diag_values = [list(v) for v in  out_histograms.shared_diagnostics]
    self.assertEqual(len(diag_values), 4)
    self.assertIn(['benchmark'], diag_values)
    self.assertIn(['linux'], diag_values)
    self.assertIn([['documentation', 'url']], diag_values)
    self.assertIn(['label'], diag_values)

  def testHtmlOutputResetResults(self):
    self.SerializeIntermediateResults([])

    processor.main([
        '--output-format', 'html',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label1',
    ])

    processor.main([
        '--output-format', 'html',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label2',
        '--reset-results',
    ])

    with open(os.path.join(
        self.output_dir, html_output.OUTPUT_FILENAME)) as f:
      results = render_histograms_viewer.ReadExistingResults(f.read())

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)
    diag_values = [list(v) for v in  out_histograms.shared_diagnostics]
    self.assertNotIn(['label1'], diag_values)
    self.assertIn(['label2'], diag_values)

  def testHtmlOutputAppendResults(self):
    self.SerializeIntermediateResults([])

    processor.main([
        '--output-format', 'html',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label1',
    ])

    processor.main([
        '--output-format', 'html',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label2',
    ])

    with open(os.path.join(
        self.output_dir, html_output.OUTPUT_FILENAME)) as f:
      results = render_histograms_viewer.ReadExistingResults(f.read())

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)
    diag_values = [list(v) for v in  out_histograms.shared_diagnostics]
    self.assertIn(['label1'], diag_values)
    self.assertIn(['label2'], diag_values)
