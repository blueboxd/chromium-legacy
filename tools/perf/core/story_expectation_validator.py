#!/usr/bin/env vpython
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to check validity of StoryExpectations."""

import logging
import os

from core import benchmark_utils
from core import benchmark_finders
from core import path_util
path_util.AddTelemetryToPath()
path_util.AddAndroidPylibToPath()

from typ import expectations_parser as typ_expectations_parser


CLUSTER_TELEMETRY_DIR = os.path.join(
    path_util.GetChromiumSrcDir(), 'tools', 'perf', 'contrib',
    'cluster_telemetry')
CLUSTER_TELEMETRY_BENCHMARKS = [
    ct_benchmark.Name() for ct_benchmark in
    benchmark_finders.GetBenchmarksInSubDirectory(CLUSTER_TELEMETRY_DIR)
]

def validate_story_names(benchmarks, test_expectations):
  stories = []
  for benchmark in benchmarks:
    if benchmark.Name() in CLUSTER_TELEMETRY_BENCHMARKS:
      continue
    story_set = benchmark_utils.GetBenchmarkStorySet(benchmark())
    stories.extend([benchmark.Name() + '/' + s.name for s in story_set.stories])
  broken_expectations = test_expectations.check_for_broken_expectations(stories)
  unused_patterns = ''
  for pattern in set([e.test for e in broken_expectations]):
    unused_patterns += ("Expectations with pattern '%s'"
                        " do not apply to any stories\n" % pattern)
  assert not unused_patterns, unused_patterns


def main():
  benchmarks = benchmark_finders.GetAllBenchmarks()
  with open(path_util.GetExpectationsPath()) as fp:
    raw_expectations_data = fp.read()
  test_expectations = typ_expectations_parser.TestExpectations()
  ret, msg = test_expectations.parse_tagged_list(raw_expectations_data)
  if ret:
    logging.error(msg)
    return ret
  validate_story_names(benchmarks, test_expectations)
  return 0
