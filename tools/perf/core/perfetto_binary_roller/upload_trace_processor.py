#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys

# Add tools/perf to sys.path.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from core import path_util
path_util.AddPyUtilsToPath()
path_util.AddTracingToPath()

from core.perfetto_binary_roller import binary_deps_manager
from core.tbmv3 import trace_processor


def _PerfettoRevision():
  perfetto_dir = os.path.join(path_util.GetChromiumSrcDir(), 'third_party',
                              'perfetto')
  revision = subprocess.check_output(
      ['git', '-C', perfetto_dir, 'rev-parse', 'HEAD'])
  return revision.strip()


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--path', help='Path to trace_processor_shell binary.', required=True)
  parser.add_argument(
      '--revision',
      help=('Perfetto revision. '
            'If not supplied, will try to infer from //third_party/perfetto.'))
  args = parser.parse_args(args)

  revision = args.revision or _PerfettoRevision()

  binary_deps_manager.UploadHostBinary(trace_processor.TP_BINARY_NAME,
                                       args.path, revision)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
