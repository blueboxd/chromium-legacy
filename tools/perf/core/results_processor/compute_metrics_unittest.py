# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from core.results_processor import compute_metrics
from core.results_processor import testing

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
        run_metrics_mock.assert_not_called()

    self.assertEqual(histogram_dicts, [])
