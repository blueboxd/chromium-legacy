# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from core.results_processor import compute_metrics
from core.results_processor import testing

from tracing.mre import failure
from tracing.mre import job
from tracing.mre import mre_result
from tracing.value import histogram

import mock


RUN_METRICS_METHOD = 'tracing.metrics.metric_runner.RunMetricOnSingleTrace'
GETSIZE_METHOD = 'os.path.getsize'


class ComputeMetricsTest(unittest.TestCase):
  def testComputeTBMv2Metrics(self):
    in_results = testing.IntermediateResults([
        testing.TestResult(
            'benchmark/story1',
            artifacts={
                compute_metrics.HTML_TRACE_NAME:
                    testing.Artifact('/trace1.html', 'gs://trace1.html')},
            tags=['tbmv2:metric1'],
        ),
        testing.TestResult(
            'benchmark/story2',
            artifacts={
                compute_metrics.HTML_TRACE_NAME:
                    testing.Artifact('/trace2.html', 'gs://trace2.html')},
            tags=['tbmv2:metric2'],
        ),
    ])

    test_dict = histogram.Histogram('a', 'unitless').AsDict()
    metrics_result = mre_result.MreResult()
    metrics_result.AddPair('histograms', [test_dict])

    with mock.patch(GETSIZE_METHOD) as getsize_mock:
      with mock.patch(RUN_METRICS_METHOD) as run_metrics_mock:
        getsize_mock.return_value = 1000
        run_metrics_mock.return_value = metrics_result
        histogram_dicts = compute_metrics.ComputeTBMv2Metrics(in_results)

    self.assertEqual(histogram_dicts, [test_dict, test_dict])
    self.assertEqual(in_results['testResults'][0]['status'], 'PASS')
    self.assertEqual(in_results['testResults'][1]['status'], 'PASS')

  def testComputeTBMv2MetricsTraceTooBig(self):
    in_results = testing.IntermediateResults([
        testing.TestResult(
            'benchmark/story1',
            artifacts={
                compute_metrics.HTML_TRACE_NAME:
                    testing.Artifact('/trace1.html', 'gs://trace1.html')},
            tags=['tbmv2:metric1'],
        ),
    ])

    with mock.patch(GETSIZE_METHOD) as getsize_mock:
      with mock.patch(RUN_METRICS_METHOD) as run_metrics_mock:
        getsize_mock.return_value = 1e9
        histogram_dicts = compute_metrics.ComputeTBMv2Metrics(in_results)
        self.assertEqual(run_metrics_mock.call_count, 0)

    self.assertEqual(histogram_dicts, [])
    self.assertEqual(in_results['testResults'][0]['status'], 'FAIL')

  def testComputeTBMv2MetricsFailure(self):
    in_results = testing.IntermediateResults([
        testing.TestResult(
            'benchmark/story1',
            artifacts={
                compute_metrics.HTML_TRACE_NAME:
                    testing.Artifact('/trace1.html', 'gs://trace1.html')},
            tags=['tbmv2:metric1'],
        ),
    ])

    metrics_result = mre_result.MreResult()
    metrics_result.AddFailure(failure.Failure(job.Job(0), 0, 0, 0, 0, 0))

    with mock.patch(GETSIZE_METHOD) as getsize_mock:
      with mock.patch(RUN_METRICS_METHOD) as run_metrics_mock:
        getsize_mock.return_value = 100
        run_metrics_mock.return_value = metrics_result
        histogram_dicts = compute_metrics.ComputeTBMv2Metrics(in_results)

    self.assertEqual(histogram_dicts, [])
    self.assertEqual(in_results['testResults'][0]['status'], 'FAIL')

  def testComputeTBMv2MetricsSkipped(self):
    in_results = testing.IntermediateResults([
        testing.TestResult(
            'benchmark/story1',
            artifacts={
                compute_metrics.HTML_TRACE_NAME:
                    testing.Artifact('/trace1.html', 'gs://trace1.html')},
            tags=['tbmv2:metric1'],
            status='SKIP',
        ),
    ])

    with mock.patch(RUN_METRICS_METHOD) as run_metrics_mock:
      histogram_dicts = compute_metrics.ComputeTBMv2Metrics(in_results)
      self.assertEqual(run_metrics_mock.call_count, 0)

    self.assertEqual(histogram_dicts, [])
    self.assertEqual(in_results['testResults'][0]['status'], 'SKIP')
