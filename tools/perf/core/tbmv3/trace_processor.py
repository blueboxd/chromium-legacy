# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import subprocess

from collections import namedtuple

from py_utils import tempfile_ext
from tracing.value import histogram_set


TP_BINARY_NAME = 'trace_processor_shell'
EXPORT_JSON_QUERY_TEMPLATE = 'select export_json(%s)\n'
METRICS_PATH = os.path.realpath(os.path.join(os.path.dirname(__file__),
                                             'metrics'))

MetricFiles = namedtuple('MetricFiles', ('sql', 'proto', 'config'))


def _SqlString(s):
  """Produce a valid SQL string constant."""
  return "'%s'" % s.replace("'", "''")


def _CheckTraceProcessor(trace_processor_path):
  if trace_processor_path is None:
    raise RuntimeError('Trace processor executable is not supplied. '
                       'Please use the --trace-processor-path flag.')
  if not os.path.isfile(trace_processor_path):
    raise RuntimeError("Can't find trace processor executable at %s" %
                       trace_processor_path)


def _CreateMetricFiles(metric_name):
  # Currently assuming all metric files live in tbmv3/metrics directory. We will
  # revise this decision later.
  metric_files = MetricFiles(
      sql=os.path.join(METRICS_PATH, metric_name + '.sql'),
      proto=os.path.join(METRICS_PATH, metric_name + '.proto'),
      config=os.path.join(METRICS_PATH, metric_name + '_config.json'))
  for filetype, path in metric_files._asdict().iteritems():
    if not os.path.isfile(path):
      raise RuntimeError('metric %s file not found at %s' % (filetype, path))
  return metric_files


def _ScopedHistogramName(metric_name, histogram_name):
  """Returns scoped histogram name by preprending metric name.

  This is useful for avoiding histogram name collision. The '_metric' suffix of
  the metric name is dropped from scoped name. Example:
  _ScopedHistogramName("console_error_metric", "js_errors")
  => "console_error::js_errors"
  """
  metric_suffix = '_metric'
  suffix_length = len(metric_suffix)
  # TODO(crbug.com/1012687): Decide on whether metrics should always have
  # '_metric' suffix.
  if metric_name[-suffix_length:] == metric_suffix:
    scope = metric_name[:-suffix_length]
  else:
    scope = metric_name
  return '::'.join([scope, histogram_name])


def RunMetric(trace_processor_path, trace_file, metric_name):
  """Run a TBMv3 metric using trace processor.

  Args:
    trace_processor_path: path to the trace_processor executable.
    trace_file: path to the trace file.
    metric_name: the metric name (the corresponding files must exist in
        tbmv3/metrics directory).

  Returns:
    A HistogramSet with metric results.
  """
  _CheckTraceProcessor(trace_processor_path)
  metric_files = _CreateMetricFiles(metric_name)
  output = subprocess.check_output([
      trace_processor_path,
      trace_file,
      '--run-metrics', metric_files.sql,
      '--metrics-output=json',
      '--extra-metrics', METRICS_PATH
  ])
  measurements = json.loads(output)

  histograms = histogram_set.HistogramSet()
  with open(metric_files.config) as f:
    config = json.load(f)
  metric_root_field = 'perfetto.protos.' + metric_name
  for histogram_config in config['histograms']:
    histogram_name = histogram_config['name']
    samples = measurements[metric_root_field][histogram_name]
    scoped_histogram_name = _ScopedHistogramName(metric_name, histogram_name)
    description = histogram_config['description']
    histograms.CreateHistogram(scoped_histogram_name,
                               histogram_config['unit'], samples,
                               description=description)
  return histograms


def ConvertProtoTraceToJson(trace_processor_path, proto_file, json_path):
  """Convert proto trace to json using trace processor.

  Args:
    trace_processor_path: path to the trace_processor executable.
    proto_file: path to the proto trace file.
    json_path: path to the output file.

  Returns:
    Output path.
  """
  _CheckTraceProcessor(trace_processor_path)
  with tempfile_ext.NamedTemporaryFile() as query_file:
    query_file.write(EXPORT_JSON_QUERY_TEMPLATE % _SqlString(json_path))
    query_file.close()
    subprocess.check_call([
        trace_processor_path,
        proto_file,
        '-q', query_file.name,
    ])

  logging.info('Converted json trace written to %s', json_path)

  return json_path
