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

import mock

from core.results_processor import json3_output
from core.results_processor import processor
from core.results_processor import testing


class ResultProcessorIntegrationTests(unittest.TestCase):
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

  # TODO(crbug.com/981349): Remove mock when results_processor is allowed to
  # handle json-test-results format.
  @mock.patch.dict('core.results_processor.processor.SUPPORTED_FORMATS',
                   {'json-test-results': json3_output})
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
