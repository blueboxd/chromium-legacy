# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import time

from tracing.metrics import metric_runner


# Aggregated trace is saved under this name.
HTML_TRACE_NAME = 'trace.html'

# Results of metric computation are stored under this key in test_results.
HISTOGRAM_DICTS_KEY = 'histogram_dicts'

# This file is written by telemetry, it contains output of metric computation.
# This is a temporary hack to keep things working while we gradually move
# code from Telemetry to Results Processor.
HISTOGRAM_DICTS_FILE = 'histogram_dicts.json'


def _RunMetric(test_result):
  metrics = [tag['value'] for tag in test_result['tags']
             if tag['key'] == 'tbmv2']
  html_trace = test_result['outputArtifacts'][HTML_TRACE_NAME]
  html_local_path = html_trace['filePath']
  html_remote_url = html_trace.get('remoteUrl')

  logging.info('%s: Starting to compute metrics on trace.',
               test_result['testPath'])
  start = time.time()
  # The timeout needs to be coordinated with the Swarming IO timeout for the
  # task that runs this code. If this timeout is longer or close in length
  # to the swarming IO timeout then we risk being forcibly killed for not
  # producing any output. Note that this could be fixed by periodically
  # outputting logs while waiting for metrics to be calculated.
  TEN_MINUTES = 60 * 10
  mre_result = metric_runner.RunMetricOnSingleTrace(
      html_local_path, metrics, canonical_url=html_remote_url,
      timeout=TEN_MINUTES,
      extra_import_options={'trackDetailedModelStats': True})
  logging.info('%s: Computing metrics took %.3f seconds.' % (
      test_result['testPath'], time.time() - start))

  if mre_result.failures:
    test_result['status'] = 'FAIL'
    for f in mre_result.failures:
      logging.error('Failure recorded for test %s: %s',
                    test_result['testPath'], f)

  return mre_result.pairs.get('histograms', [])


def ComputeTBMv2Metrics(test_result):
  """Compute metrics on aggregated traces in parallel.

  For each test run that has an aggregate trace and some TBMv2 metrics listed
  in its tags, compute the metrics and return the list of all resulting
  histograms. Note: the order of histograms in the results may be different
  from the order of tests in intermediate_results.
  """
  artifacts = test_result.get('outputArtifacts', {})
  # TODO(crbug.com/981349): If metrics have already been computed in
  # Telemetry, we read it from the file. Remove this branch after Telemetry
  # does not compute metrics anymore.
  if HISTOGRAM_DICTS_FILE in artifacts:
    with open(artifacts[HISTOGRAM_DICTS_FILE]['filePath']) as f:
      test_result['_histograms'].ImportDicts(json.load(f))
    del artifacts[HISTOGRAM_DICTS_FILE]
    return

  if test_result['status'] == 'SKIP':
    return

  if (HTML_TRACE_NAME not in artifacts or
      not any(tag['key'] == 'tbmv2' for tag in test_result.get('tags', []))):
    return

  trace_size_in_mib = (os.path.getsize(artifacts[HTML_TRACE_NAME]['filePath'])
                       / (2 ** 20))
  # Bails out on traces that are too big. See crbug.com/812631 for more
  # details.
  # TODO(crbug.com/1010041): Return a non-zero exit code in this case.
  if trace_size_in_mib > 400:
    test_result['status'] = 'FAIL'
    logging.error('%s: Trace size is too big: %s MiB',
                  test_result['testPath'], trace_size_in_mib)
    return

  test_result['_histograms'].ImportDicts(_RunMetric(test_result))
