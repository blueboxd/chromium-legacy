# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys


def main():
  description = 'Packages WebKit build for Clusterfuzz.'
  parser = argparse.ArgumentParser(description=description)
  parser.add_argument('--output',
                    help='Output directory for build products.')
  parser.add_argument('--webkit_build',
                      help='WebKit build directory to copy.')
  parser.add_argument('--clusterfuzz_script',
                      help='Clusterfuzz script to copy.')

  opts = parser.parse_args()

  if os.path.exists(opts.output):
    shutil.rmtree(opts.output)

  shutil.copytree(opts.webkit_build, opts.output, symlinks=True)
  shutil.copyfile(
        opts.clusterfuzz_script,
        os.path.join(opts.output,
                     os.path.basename(opts.clusterfuzz_script)))


if __name__ == '__main__':
  sys.exit(main())
