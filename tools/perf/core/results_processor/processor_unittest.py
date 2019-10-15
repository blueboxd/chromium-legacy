# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for results_processor methods."""

import os
import unittest

import mock

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

  def testUploadArtifacts(self):
    in_results = testing.IntermediateResults(
        test_results=[
            testing.TestResult(
                'benchmark/story',
                output_artifacts={'log': testing.Artifact('/log.log')},
            ),
            testing.TestResult(
                'benchmark/story',
                output_artifacts={
                  'trace.html': testing.Artifact('/trace.html'),
                  'screenshot': testing.Artifact('/screenshot.png'),
                },
            ),
        ],
    )

    with mock.patch('py_utils.cloud_storage.Insert') as cloud_patch:
      cloud_patch.return_value = 'gs://url'
      processor.UploadArtifacts(in_results, 'bucket', None)
      cloud_patch.assert_has_calls([
          mock.call('bucket', mock.ANY, '/log.log'),
          mock.call('bucket', mock.ANY, '/trace.html'),
          mock.call('bucket', mock.ANY, '/screenshot.png'),
        ],
        any_order=True,
      )

    for result in in_results['testResults']:
      for artifact in result['outputArtifacts'].itervalues():
        self.assertEqual(artifact['remoteUrl'], 'gs://url')

  def testUploadArtifacts_CheckRemoteUrl(self):
    in_results = testing.IntermediateResults(
        test_results=[
            testing.TestResult(
                'benchmark/story',
                output_artifacts={
                    'trace.html': testing.Artifact('/trace.html')
                },
            ),
        ],
        start_time='2019-10-01T12:00:00.123456Z',
    )

    with mock.patch('py_utils.cloud_storage.Insert') as cloud_patch:
      with mock.patch('random.randint') as randint_patch:
        randint_patch.return_value = 54321
        processor.UploadArtifacts(in_results, 'bucket', 'src@abc + 123')
        cloud_patch.assert_called_once_with(
            'bucket',
            'src_abc_123_20191001T120000_54321/benchmark/story/trace.html',
            '/trace.html'
        )

  def testAggregateTraces(self):
    in_results = testing.IntermediateResults(
        test_results=[
            testing.TestResult(
                'benchmark/story1',
                output_artifacts={
                    'trace/1.json': testing.Artifact(
                        os.path.join('test_run', 'story1', 'trace', '1.json')),
                },
            ),
            testing.TestResult(
                'benchmark/story2',
                output_artifacts={
                    'trace/1.json': testing.Artifact(
                        os.path.join('test_run', 'story2', 'trace', '1.json')),
                    'trace/2.json': testing.Artifact(
                        os.path.join('test_run', 'story2', 'trace', '2.json')),
                },
            ),
        ],
    )

    with mock.patch('tracing.trace_data.trace_data.SerializeAsHtml') as patch:
      processor.AggregateTraces(in_results)

    call_list = [list(call[0]) for call in patch.call_args_list]
    self.assertEqual(len(call_list), 2)
    for call in call_list:
      call[0] = set(call[0])
    self.assertIn(
        [
            set([os.path.join('test_run', 'story1', 'trace', '1.json')]),
            os.path.join('test_run', 'story1', 'trace', 'trace.html'),
        ],
        call_list
    )
    self.assertIn(
        [
            set([
                os.path.join('test_run', 'story2', 'trace', '1.json'),
                os.path.join('test_run', 'story2', 'trace', '2.json'),
            ]),
            os.path.join('test_run', 'story2', 'trace', 'trace.html'),
        ],
        call_list
    )

    for result in in_results['testResults']:
      artifacts = result['outputArtifacts']
      self.assertEqual(len(artifacts), 1)
      self.assertEqual(artifacts.keys()[0], 'trace.html')

  def testMeasurementToHistogram(self):
    hist = processor.MeasurementToHistogram('a', {
      'unit': 'sizeInBytes',
      'samples': [1, 2, 3],
      'description': 'desc',
    })

    self.assertEqual(hist.name, 'a')
    self.assertEqual(hist.unit, 'sizeInBytes')
    self.assertEqual(hist.sample_values, [1, 2, 3])
    self.assertEqual(hist.description, 'desc')

  def testMeasurementToHistogramLegacyUnits(self):
    hist = processor.MeasurementToHistogram('a', {
      'unit': 'seconds',
      'samples': [1, 2, 3],
    })

    self.assertEqual(hist.name, 'a')
    self.assertEqual(hist.unit, 'ms_smallerIsBetter')
    self.assertEqual(hist.sample_values, [1000, 2000, 3000])

  def testMeasurementToHistogramUnknownUnits(self):
    with self.assertRaises(ValueError):
      processor.MeasurementToHistogram('a', {'unit': 'yards', 'samples': [9]})
