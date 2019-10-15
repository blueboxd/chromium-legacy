#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import subprocess
import sys

from collections import namedtuple


_CHROMIUM_SRC_PATH  = os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..')
_DEFAULT_TP_PATH = os.path.abspath(os.path.join(
    _CHROMIUM_SRC_PATH, 'out', 'Debug', 'trace_processor_shell'))
_METRICS_PATH = os.path.join(os.path.dirname(__file__), 'metrics')

MetricFiles = namedtuple('MetricFiles', ('sql', 'proto'))


class MetricFileNotFound(Exception):
  pass


class TraceProcessorNotFound(Exception):
  pass


class TraceProcessorError(Exception):
  pass


def _CheckFilesExist(trace_processor_path, metric_files):
  if not os.path.exists(trace_processor_path):
    raise TraceProcessorNotFound("Could not find trace processor shell at %s"
                                 % trace_processor_path)

  # Currently assuming all metric files live in tbmv3/metrics directory. We will
  # revise this decision later.
  for filetype, path in metric_files._asdict().iteritems():
    if not os.path.exists(path):
      raise MetricFileNotFound("metric %s file not found at %s"
                               % (filetype, path))


def Main():
  parser = argparse.ArgumentParser(
    description='[Experimental] Runs TBMv3 metrics on local traces.')
  parser.add_argument('--trace', required=True,
                      help='Trace file you want to compute metric on')
  parser.add_argument('--metric', required=True,
                      help=('Name of the metric you want to run'))
  parser.add_argument('--trace_processor_path', default=_DEFAULT_TP_PATH,
                      help='Path to trace processor shell. '
                      'Default: %(default)s')
  args = parser.parse_args()

  metric_files = MetricFiles(
    sql=os.path.join(_METRICS_PATH, args.metric + '.sql'),
    proto=os.path.join(_METRICS_PATH, args.metric + '.proto')
  )
  _CheckFilesExist(args.trace_processor_path, metric_files)

  trace_processor_args = [args.trace_processor_path,
                          args.trace,
                          '--run-metrics', metric_files.sql,
                          '--metrics-output=json',
                          '--extra-metrics', _METRICS_PATH]

  try:
    json_output = subprocess.check_output(trace_processor_args)
  except subprocess.CalledProcessError:
    raise TraceProcessorError("Failed to compute metrics. " +
                              "Check trace processor logs.")

  json_result = json.loads(json_output)
  print "Metric result:"
  print json_result


if __name__ == '__main__':
  sys.exit(Main())
