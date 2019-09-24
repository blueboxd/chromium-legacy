# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Output formatter for HistogramSet Results Format.

Format specification:
https://github.com/catapult-project/catapult/blob/master/docs/histogram-set-json-format.md
"""

import json
import logging
import os

from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import reserved_infos
from tracing.value import histogram_set


# This file is written by telemetry, it contains output of metric computation.
# This is a temporary hack to keep things working while we gradually move
# code from Telemetry to Results Processor.
HISTOGRAM_DICTS_NAME = 'histogram_dicts.json'

# Output file in HistogramSet format.
OUTPUT_FILENAME = 'histograms.json'


def Process(intermediate_results, options):
  """Process intermediate results and write output in output_dir."""
  histogram_dicts = Convert(intermediate_results, options.results_label)

  output_file = os.path.join(options.output_dir, OUTPUT_FILENAME)
  if not options.reset_results and os.path.isfile(output_file):
    with open(output_file) as input_stream:
      try:
        histogram_dicts += json.load(input_stream)
      except ValueError:
        logging.warning(
            'Found existing histograms json but failed to parse it.')

  with open(output_file, 'w') as output_stream:
    json.dump(histogram_dicts, output_stream)


def Convert(intermediate_results, results_label):
  """Convert intermediate results to histogram dicts"""
  histograms = histogram_set.HistogramSet()
  for result in intermediate_results['testResults']:
    if HISTOGRAM_DICTS_NAME in result['artifacts']:
      with open(result['artifacts'][HISTOGRAM_DICTS_NAME]['filePath']) as f:
        histogram_dicts = json.load(f)
      histograms.ImportDicts(histogram_dicts)

  diagnostics = intermediate_results['benchmarkRun'].get('diagnostics', {})
  for name, diag in diagnostics.items():
    # For now, we only support GenericSet diagnostics that are serialized
    # as lists of values.
    assert isinstance(diag, list)
    histograms.AddSharedDiagnosticToAllHistograms(
        name, generic_set.GenericSet(diag))

  if results_label is not None:
    histograms.AddSharedDiagnosticToAllHistograms(
        reserved_infos.LABELS.name,
        generic_set.GenericSet([results_label]))

  return histograms.AsDicts()
