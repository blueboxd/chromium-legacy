#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script compresses a part of shared library without breaking it.

It compresses the specified file range which will be used by a new library
constructor that uses userfaultfd to intercept access attempts to the range
and decompress its data on demand.

Technically this script does the following steps:
  1) Makes a copy of specified range, compresses it and adds it to the binary
    as a new section using objcopy.
  2) Moves the Phdr section to the end of the file to ease next manipulations on
    it.
  3) Creates a LOAD segment for the compressed section created at step 1.
  4) Splits the LOAD segments so the range lives in its own
    LOAD segment.
  5) Cuts the range out of the binary, correcting offsets, broken in the
    process.
  6) Changes cut range LOAD segment by zeroing the file_sz and setting
    mem_sz to the original range size, essentially lazily zeroing the space.
  7) Changes address of the symbols provided by the library constructor to
    point to the cut range and compressed section.
"""

import argparse
import logging
import sys


def _SetupLogging():
  logging.basicConfig(
      format='%(asctime)s %(filename)s:%(lineno)s %(levelname)s] %(message)s',
      datefmt='%H:%M:%S',
      level=logging.ERROR)


def _ParseArguments():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-i', '--input', help='Shared library to parse', required=True)
  parser.add_argument(
      '-o', '--output', help='Name of the output file', required=True)
  parser.add_argument(
      '-l',
      '--left_range',
      help='Beginning of the target part of the library',
      type=int,
      required=True)
  parser.add_argument(
      '-r',
      '--right_range',
      help='End (inclusive) of the target part of the library',
      type=int,
      required=True)
  return parser.parse_args()


def main():
  _SetupLogging()
  args = _ParseArguments()

  with open(args.input, 'rb') as f:
    data = f.read()
    data = bytearray(data)

  with open(args.output, 'wb') as f:
    f.write(data)
  return 0


if __name__ == '__main__':
  sys.exit(main())
