# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import tempfile
import unittest

from core.tbmv3 import trace_processor

import mock


class TraceProcessorTestCase(unittest.TestCase):
  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()
    self.tp_path = os.path.join(self.temp_dir, 'trace_processor_shell')
    with open(self.tp_path, 'w'):
      pass
    with open(os.path.join(self.temp_dir, 'dummy_metric.sql'), 'w'):
      pass
    with open(os.path.join(self.temp_dir, 'dummy_metric.proto'), 'w'):
      pass
    with open(os.path.join(self.temp_dir,
                           'dummy_metric_config.json'), 'w') as config:
      json.dump(
          {
              'name': 'Dummy Metric',
              'histograms': [{
                'name': 'value',
                'description': 'dummy value',
                'unit': 'count_smallerIsBetter',
              }],
          },
          config,
      )

  def tearDown(self):
    shutil.rmtree(self.temp_dir)

  def testConvertProtoTraceToJson(self):
    with mock.patch('subprocess.check_call'):
      trace_processor.ConvertProtoTraceToJson(
          self.tp_path, '/path/to/proto', '/path/to/json')

  def testRunMetric(self):
    metric_output = '{"perfetto.protos.dummy_metric": {"value": 7}}'

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch('subprocess.check_output') as check_output_patch:
        check_output_patch.return_value = metric_output
        histograms = trace_processor.RunMetric(
            self.tp_path, '/path/to/proto', 'dummy_metric')

    hist = histograms.GetHistogramNamed('dummy::value')
    self.assertEqual(hist.unit, 'count_smallerIsBetter')
    self.assertEqual(hist.sample_values, [7])

