# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

# Add src/testing/ into sys.path for importing representative perf test script.
PERF_TEST_SCRIPTS_DIR = os.path.join(
    os.path.dirname(__file__), '..', '..', 'testing', 'scripts')
sys.path.append(PERF_TEST_SCRIPTS_DIR)
import run_rendering_benchmark_with_gated_performance as perf_tests  # pylint: disable=wrong-import-position,import-error

UPPER_LIMIT_DATA_SAMPLE = {
    'story_1': {
        'ci_095': 10,
        'avg': 20
    },
    'story_2': {
        'ci_095': 10,
        'avg': 16
    },
    'story_3': {
        'ci_095': 10,
        'avg': 10
    },
    'story_4': {
        'ci_095': 10,
        'avg': 10,
        'control': True,
    },
    'story_5': {
        'ci_095': 20,
        'avg': 10
    },
    'story_6': {
        'ci_095': 20,
        'avg': 10
    },
}


def create_sample_input(record_list):
  # Coverts an array of arrays in to an array of dicts with keys of
  # stories, name, avg, count, ci_095 for the unittests.
  keys = ['stories', 'name', 'avg', 'count', 'ci_095']
  result = []
  for row in record_list:
    result.append(dict(zip(keys, row)))
  return result


def create_sample_perf_results(passed_stories, failed_stories, benchmark):
  perf_results = {
      'tests': {},
      'num_failures_by_type': {
          'FAIL': len(failed_stories),
          'PASS': len(passed_stories)
      }
  }
  perf_results['tests'][benchmark] = {}
  for story in passed_stories:
    perf_results['tests'][benchmark][story] = {
        'actual': 'PASS',
        'is_unexpected': False,
        'expected': 'PASS'
    }
  for story in failed_stories:
    perf_results['tests'][benchmark][story] = {
        'actual': 'FAIL',
        'is_unexpected': True,
        'expected': 'PASS'
    }

  return perf_results


class TestRepresentativePerfScript(unittest.TestCase):
  def test_parse_csv_results(self):
    csv_obj = create_sample_input([
        ['story_1', 'frame_times', 16, 10, 1.5],
        ['story_2', 'latency', 10, 8, 4],  # Record for a different metric.
        ['story_3', 'frame_times', 8, 20, 2],
        ['story_4', 'frame_times', '', 10, 1],  # Record with no avg.
        ['story_5', 'frame_times', 12, 0, 3],  # Record with count of 0.
        ['story_6', 'frame_times', 12, 40, 40],  # High noise record.
        ['story_7', 'frame_times', 12, 40, 4],
        ['story_3', 'frame_times', 7, 20, 15],
        ['story_3', 'frame_times', 12, 20, 16]
    ])
    values_per_story = perf_tests.parse_csv_results(csv_obj,
                                                    UPPER_LIMIT_DATA_SAMPLE)
    # Existing Frame_times stories in upper_limits should be listed.
    # All stories but story_2 & story_7.
    self.assertEquals(len(values_per_story), 5)
    self.assertEquals(values_per_story['story_1']['averages'], [16.0])
    self.assertEquals(values_per_story['story_1']['ci_095'], [1.5])

    # Record with avg 12 has high noise.
    self.assertEquals(values_per_story['story_3']['averages'], [8.0, 7.0, 12.0])
    self.assertEquals(values_per_story['story_3']['ci_095'], [2.0, 15.0, 16.0])

    self.assertEquals(len(values_per_story['story_4']['averages']), 0)
    self.assertEquals(len(values_per_story['story_4']['ci_095']), 0)
    self.assertEquals(len(values_per_story['story_5']['averages']), 0)
    self.assertEquals(len(values_per_story['story_5']['ci_095']), 0)
    self.assertEquals(values_per_story['story_6']['averages'], [12.0])
    self.assertEquals(values_per_story['story_6']['ci_095'], [40.0])

  def test_compare_values_1(self):
    values_per_story = {
        'story_1': {
            'averages': [16.0, 17.0, 21.0],
            'ci_095': [2.0, 15.0, 16.0],
        },
        'story_2': {
            'averages': [16.0, 17.0, 22.0],
            'ci_095': [1.0, 1.4, 1.2],
        }
    }
    benchmark = 'rendering.desktop'

    sample_perf_results = create_sample_perf_results(['story_1', 'story_2'], [],
                                                     benchmark)

    result_recorder = perf_tests.ResultRecorder()
    result_recorder.set_tests(sample_perf_results)

    result_recorder = perf_tests.compare_values(
        values_per_story, UPPER_LIMIT_DATA_SAMPLE, benchmark, result_recorder)
    self.assertEquals(result_recorder.tests, 2)
    self.assertEquals(result_recorder.failed_stories, set(['story_2']))
    (output, overall_return_code) = result_recorder.get_output(0)
    self.assertEquals(overall_return_code, 1)
    self.assertEquals(output['num_failures_by_type'].get('FAIL', 0), 1)
    self.assertEquals(output['tests'][benchmark]['story_1']['actual'], 'PASS')
    self.assertEquals(output['tests'][benchmark]['story_2']['actual'], 'FAIL')

  def test_compare_values_2(self):
    values_per_story = {
      'story_1': {
        'averages': [16.0, 17.0, 21.0],
        'ci_095': [2.0, 15.0, 16.0],
      },
      'story_3': { # Two of the runs have acceptable CI but high averages.
        'averages': [10, 13],
        'ci_095': [14, 16, 12]
      },
      'story_4': {  # All runs have high noise.
        'averages': [],
        'ci_095': [16, 17, 18],
      },
      'story_5': {  # No recorded values.
        'averages': [],
        'ci_095': [],
      }
    }

    benchmark = 'rendering.desktop'

    sample_perf_results = create_sample_perf_results(
        ['story_1', 'story_3', 'story_4', 'story_5'], ['story_2'], benchmark)

    result_recorder = perf_tests.ResultRecorder()
    result_recorder.set_tests(sample_perf_results)
    self.assertEquals(result_recorder.fails, 1)

    result_recorder = perf_tests.compare_values(
        values_per_story, UPPER_LIMIT_DATA_SAMPLE, benchmark, result_recorder)
    self.assertEquals(result_recorder.tests, 5)
    self.assertEquals(result_recorder.failed_stories,
                      set(['story_3', 'story_4', 'story_5']))
    self.assertTrue(result_recorder.is_control_stories_noisy)

    result_recorder.invalidate_failures(benchmark)
    (output, overall_return_code) = result_recorder.get_output(0)

    self.assertEquals(overall_return_code, 0)
    self.assertEquals(output['num_failures_by_type'].get('FAIL', 0), 0)
    self.assertEquals(output['tests'][benchmark]['story_1']['actual'], 'PASS')
    self.assertEquals(output['tests'][benchmark]['story_3']['actual'], 'FAIL')
    self.assertEquals(output['tests'][benchmark]['story_4']['actual'], 'FAIL')
