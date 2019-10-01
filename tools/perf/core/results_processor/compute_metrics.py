# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json


# This file is written by telemetry, it contains output of metric computation.
# This is a temporary hack to keep things working while we gradually move
# code from Telemetry to Results Processor.
HISTOGRAM_DICTS_FILE = 'histogram_dicts.json'


def ComputeTBMv2Metrics(intermediate_results):
  """Compute metrics on aggregated traces in parallel.

  For each test run that has an aggregate trace and some TBMv2 metrics listed
  in its tags, compute the metrics and store the result as histogram dicts
  in the corresponding test result.
  """
  histogram_dicts = []
  for test_result in intermediate_results['testResults']:
    artifacts = test_result.get('artifacts', {})
    # For now, metrics are computed in telemetry.
    # TODO(crbug.com/981349): Replace it with actual metrics computation.
    assert HISTOGRAM_DICTS_FILE in artifacts
    with open(artifacts[HISTOGRAM_DICTS_FILE]['filePath']) as f:
      histogram_dicts += json.load(f)

  return histogram_dicts

