# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Output formatter for HTML Results format."""

import codecs
import os

from core.results_processor.formatters import histograms_output
from tracing_build import vulcanize_histograms_viewer


OUTPUT_FILENAME = 'results.html'


def Process(intermediate_results, options):
  """Process intermediate results and write output in output_dir."""
  histogram_dicts = histograms_output.Convert(intermediate_results,
                                              options.results_label)

  output_file = os.path.join(options.output_dir, OUTPUT_FILENAME)
  open(output_file, 'a').close()  # Create file if it doesn't exist.
  with codecs.open(output_file, mode='r+', encoding='utf-8') as output_stream:
    vulcanize_histograms_viewer.VulcanizeAndRenderHistogramsViewer(
        histogram_dicts, output_stream, options.reset_results)
