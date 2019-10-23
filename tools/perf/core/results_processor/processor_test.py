# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Integration tests for results_processor.

These tests write actual files with intermediate results, and run the
standalone results processor on them to check that output files are produced
with their expected contents.
"""

import csv
import datetime
import json
import os
import shutil
import tempfile
import unittest

from core.results_processor.formatters import csv_output
from core.results_processor.formatters import json3_output
from core.results_processor.formatters import histograms_output
from core.results_processor.formatters import html_output
from core.results_processor import compute_metrics
from core.results_processor import processor
from core.results_processor import testing

from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import date_range
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

  def SerializeIntermediateResults(self, *test_results):
    testing.SerializeIntermediateResults(test_results, os.path.join(
        self.intermediate_dir, processor.TELEMETRY_RESULTS))

  def CreateHistogramsArtifact(self, hist):
    """Create an artifact with histograms."""
    histogram_dicts = [hist.AsDict()]
    hist_file = os.path.join(self.output_dir,
                             compute_metrics.HISTOGRAM_DICTS_FILE)
    with open(hist_file, 'w') as f:
      json.dump(histogram_dicts, f)
    return testing.Artifact(hist_file)

  def CreateDiagnosticsArtifact(self, **diagnostics):
    """Create an artifact with diagnostics."""
    diag_file = os.path.join(self.output_dir,
                             processor.DIAGNOSTICS_NAME)
    with open(diag_file, 'w') as f:
      json.dump({'diagnostics': diagnostics}, f)
    return testing.Artifact(diag_file)

  def testJson3Output(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story', run_duration='1.1s', tags=['shard:7'],
            start_time='2009-02-13T23:31:30.987000Z'),
        testing.TestResult(
            'benchmark/story', run_duration='1.2s', tags=['shard:7']),
    )

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
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                'logs': testing.Artifact('/logs.txt', 'gs://logs.txt'),
                'trace/telemetry': testing.Artifact('/telemetry.json'),
                'trace.html':
                    testing.Artifact('/trace.html', 'gs://trace.html'),
            }
        ),
    )

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
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                compute_metrics.HISTOGRAM_DICTS_FILE:
                    self.CreateHistogramsArtifact(
                        histogram.Histogram('a', 'unitless')),
                processor.DIAGNOSTICS_NAME:
                    self.CreateDiagnosticsArtifact(
                        benchmarks=['benchmark'],
                        osNames=['linux'],
                        documentationUrls=[['documentation', 'url']]),
            },
            start_time='2009-02-13T23:31:30.987000Z',
        ),
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

    hist = out_histograms.GetFirstHistogram()
    self.assertEqual(hist.name, 'a')
    self.assertEqual(hist.unit, 'unitless')

    self.assertEqual(hist.diagnostics['benchmarks'],
                     generic_set.GenericSet(['benchmark']))
    self.assertEqual(hist.diagnostics['osNames'],
                     generic_set.GenericSet(['linux']))
    self.assertEqual(hist.diagnostics['documentationUrls'],
                     generic_set.GenericSet([['documentation', 'url']]))
    self.assertEqual(hist.diagnostics['labels'],
                     generic_set.GenericSet(['label']))
    self.assertEqual(hist.diagnostics['benchmarkStart'],
                     date_range.DateRange(1234567890987))

  def testHistogramsOutputResetResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                compute_metrics.HISTOGRAM_DICTS_FILE:
                    self.CreateHistogramsArtifact(
                        histogram.Histogram('a', 'unitless')),
            },
        ),
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

    hist = out_histograms.GetFirstHistogram()
    self.assertEqual(hist.diagnostics['labels'],
                     generic_set.GenericSet(['label2']))

  def testHistogramsOutputAppendResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                compute_metrics.HISTOGRAM_DICTS_FILE:
                    self.CreateHistogramsArtifact(
                        histogram.Histogram('a', 'unitless')),
            },
        ),
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

    expected_labels = set(['label1', 'label2'])
    observed_labels = set(label for hist in out_histograms
                           for label in hist.diagnostics['labels'])
    self.assertEqual(observed_labels, expected_labels)

  def testHistogramsOutputNoMetricsFromTelemetry(self):
    trace_file = os.path.join(self.output_dir, compute_metrics.HTML_TRACE_NAME)
    with open(trace_file, 'w') as f:
      pass

    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                compute_metrics.HTML_TRACE_NAME:
                    testing.Artifact(trace_file, 'gs://trace.html')
            },
            tags=['tbmv2:sampleMetric'],
        ),
    )

    processor.main([
        '--output-format', 'histograms',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
    ])

    with open(os.path.join(
        self.output_dir, histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    # sampleMetric records a histogram with the name 'foo'.
    hist = out_histograms.GetHistogramNamed('foo')
    self.assertIsNotNone(hist)
    self.assertEqual(hist.diagnostics['traceUrls'],
                     generic_set.GenericSet(['gs://trace.html']))

  def testHistogramsOutputNoAggregatedTrace(self):
    json_trace = os.path.join(self.output_dir, 'trace.json')
    with open(json_trace, 'w') as f:
      json.dump({'traceEvents': []}, f)

    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={'trace/json': testing.Artifact(json_trace)},
            tags=['tbmv2:sampleMetric'],
        ),
    )

    processor.main([
        '--output-format', 'histograms',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
    ])

    with open(os.path.join(
        self.output_dir, histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    # sampleMetric records a histogram with the name 'foo'.
    hist = out_histograms.GetHistogramNamed('foo')
    self.assertIsNotNone(hist)
    self.assertIn('traceUrls', hist.diagnostics)

  def testHistogramsOutputMeasurements(self):
    measure_file = os.path.join(self.output_dir,
                                processor.MEASUREMENTS_NAME)
    with open(measure_file, 'w') as f:
      json.dump({'measurements': {
          'a': {'unit': 'ms', 'samples': [4, 6], 'description': 'desc_a'},
          'b': {'unit': 'ms', 'samples': [5], 'description': 'desc_b'},
      }}, f)

    start_ts = 1500000000
    start_iso = datetime.datetime.utcfromtimestamp(start_ts).isoformat() + 'Z'

    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                processor.MEASUREMENTS_NAME: testing.Artifact(measure_file)
            },
            tags=['story_tag:test'],
            start_time=start_iso,
        ),
    )

    processor.main([
        '--output-format', 'histograms',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
    ])

    with open(os.path.join(
        self.output_dir, histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)
    self.assertEqual(len(out_histograms), 2)

    hist = out_histograms.GetHistogramNamed('a')
    self.assertEqual(hist.name, 'a')
    self.assertEqual(hist.unit, 'ms_smallerIsBetter')
    self.assertEqual(hist.sample_values, [4, 6])
    self.assertEqual(hist.description, 'desc_a')
    self.assertEqual(hist.diagnostics['benchmarks'],
                     generic_set.GenericSet(['benchmark']))
    self.assertEqual(hist.diagnostics['stories'],
                     generic_set.GenericSet(['story']))
    self.assertEqual(hist.diagnostics['storyTags'],
                     generic_set.GenericSet(['test']))
    self.assertEqual(hist.diagnostics['benchmarkStart'],
                     date_range.DateRange(start_ts * 1e3))

    hist = out_histograms.GetHistogramNamed('b')
    self.assertEqual(hist.name, 'b')
    self.assertEqual(hist.unit, 'ms_smallerIsBetter')
    self.assertEqual(hist.sample_values, [5])
    self.assertEqual(hist.description, 'desc_b')
    self.assertEqual(hist.diagnostics['benchmarks'],
                     generic_set.GenericSet(['benchmark']))
    self.assertEqual(hist.diagnostics['stories'],
                     generic_set.GenericSet(['story']))
    self.assertEqual(hist.diagnostics['storyTags'],
                     generic_set.GenericSet(['test']))
    self.assertEqual(hist.diagnostics['benchmarkStart'],
                     date_range.DateRange(start_ts * 1e3))

  def testHtmlOutput(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                compute_metrics.HISTOGRAM_DICTS_FILE:
                    self.CreateHistogramsArtifact(
                        histogram.Histogram('a', 'unitless')),
                processor.DIAGNOSTICS_NAME:
                    self.CreateDiagnosticsArtifact(
                        benchmarks=['benchmark'],
                        osNames=['linux'],
                        documentationUrls=[['documentation', 'url']]),
            },
            start_time='2009-02-13T23:31:30.987000Z',
        ),
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

    hist = out_histograms.GetFirstHistogram()
    self.assertEqual(hist.name, 'a')
    self.assertEqual(hist.unit, 'unitless')

    self.assertEqual(hist.diagnostics['benchmarks'],
                     generic_set.GenericSet(['benchmark']))
    self.assertEqual(hist.diagnostics['osNames'],
                     generic_set.GenericSet(['linux']))
    self.assertEqual(hist.diagnostics['documentationUrls'],
                     generic_set.GenericSet([['documentation', 'url']]))
    self.assertEqual(hist.diagnostics['labels'],
                     generic_set.GenericSet(['label']))
    self.assertEqual(hist.diagnostics['benchmarkStart'],
                     date_range.DateRange(1234567890987))

  def testHtmlOutputResetResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                compute_metrics.HISTOGRAM_DICTS_FILE:
                    self.CreateHistogramsArtifact(
                        histogram.Histogram('a', 'unitless')),
            },
        ),
    )

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
    self.assertEqual(len(out_histograms), 1)

    hist = out_histograms.GetFirstHistogram()
    self.assertEqual(hist.diagnostics['labels'],
                     generic_set.GenericSet(['label2']))

  def testHtmlOutputAppendResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                compute_metrics.HISTOGRAM_DICTS_FILE:
                    self.CreateHistogramsArtifact(
                        histogram.Histogram('a', 'unitless')),
            },
        ),
    )

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
    self.assertEqual(len(out_histograms), 2)

    expected_labels = set(['label1', 'label2'])
    observed_labels = set(label for hist in out_histograms
                           for label in hist.diagnostics['labels'])
    self.assertEqual(observed_labels, expected_labels)

  def testCsvOutput(self):
    test_hist = histogram.Histogram('a', 'ms')
    test_hist.AddSample(3000)
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                compute_metrics.HISTOGRAM_DICTS_FILE:
                    self.CreateHistogramsArtifact(test_hist),
                processor.DIAGNOSTICS_NAME:
                    self.CreateDiagnosticsArtifact(
                        benchmarks=['benchmark'],
                        osNames=['linux'],
                        documentationUrls=[['documentation', 'url']]),
            },
            start_time='2009-02-13T23:31:30.987000Z',
        ),
    )

    processor.main([
        '--output-format', 'csv',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label',
    ])

    with open(os.path.join(self.output_dir, csv_output.OUTPUT_FILENAME)) as f:
      lines = [line for line in f]

    actual = list(zip(*csv.reader(lines)))
    expected = [
        ('name', 'a'), ('unit', 'ms'), ('avg', '3000'), ('count', '1'),
        ('max', '3000'), ('min', '3000'), ('std', '0'), ('sum', '3000'),
        ('architectures', ''), ('benchmarks', 'benchmark'),
        ('benchmarkStart', '2009-02-13 23:31:30'), ('bots', ''),
        ('builds', ''), ('deviceIds', ''), ('displayLabel', 'label'),
        ('masters', ''), ('memoryAmounts', ''), ('osNames', 'linux'),
        ('osVersions', ''), ('productVersions', ''),
        ('stories', ''), ('storysetRepeats', ''),
        ('traceStart', ''), ('traceUrls', '')
    ]
    self.assertEqual(actual, expected)

  def testCsvOutputResetResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                compute_metrics.HISTOGRAM_DICTS_FILE:
                    self.CreateHistogramsArtifact(
                        histogram.Histogram('a', 'unitless')),
            },
        ),
    )

    processor.main([
        '--output-format', 'csv',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label1',
    ])

    processor.main([
        '--output-format', 'csv',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label2',
        '--reset-results',
    ])

    with open(os.path.join(self.output_dir, csv_output.OUTPUT_FILENAME)) as f:
      lines = [line for line in f]

    self.assertEqual(len(lines), 2)
    self.assertIn('label2', lines[1])

  def testCsvOutputAppendResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={
                compute_metrics.HISTOGRAM_DICTS_FILE:
                    self.CreateHistogramsArtifact(
                        histogram.Histogram('a', 'unitless')),
            },
        ),
    )

    processor.main([
        '--output-format', 'csv',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label1',
    ])

    processor.main([
        '--output-format', 'csv',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--results-label', 'label2',
    ])

    with open(os.path.join(self.output_dir, csv_output.OUTPUT_FILENAME)) as f:
      lines = [line for line in f]

    self.assertEqual(len(lines), 3)
    self.assertIn('label2', lines[1])
    self.assertIn('label1', lines[2])

  def testExitCodeHasFailures(self):
    self.SerializeIntermediateResults(
        testing.TestResult('benchmark/story', status='PASS'),
        testing.TestResult('benchmark/story', status='FAIL'),
    )

    exit_code = processor.main([
        '--output-format', 'json-test-results',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir])

    self.assertEqual(exit_code, 1)

  def testExitCodeAllSkipped(self):
    self.SerializeIntermediateResults(
        testing.TestResult('benchmark/story', status='SKIP'),
        testing.TestResult('benchmark/story', status='SKIP'),
    )

    exit_code = processor.main([
        '--output-format', 'json-test-results',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir])

    self.assertEqual(exit_code, -1)

  def testExitCodeSomeSkipped(self):
    self.SerializeIntermediateResults(
        testing.TestResult('benchmark/story', status='SKIP'),
        testing.TestResult('benchmark/story', status='PASS'),
    )

    exit_code = processor.main([
        '--output-format', 'json-test-results',
        '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir])

    self.assertEqual(exit_code, 0)
