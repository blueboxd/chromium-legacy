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
import os
import pathlib
import subprocess
import sys
import tempfile

import compression
import elf_headers

COMPRESSED_SECTION_NAME = '.compressed_library_data'
ADDRESS_ALIGN = 0x1000

# src/third_party/llvm-build/Release+Asserts/bin/llvm-objcopy
OBJCOPY_PATH = pathlib.Path(__file__).resolve().parents[3].joinpath(
    'third_party/llvm-build/Release+Asserts/bin/llvm-objcopy')


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
      help='End (exclusive) of the target part of the library',
      type=int,
      required=True)
  return parser.parse_args()


def _FileRangeToVirtualAddressRange(data, l, r):
  """Returns virtual address range corresponding to given file range.

  Since we have to resolve them by their virtual address, parsing of LOAD
  segments is required here.
  """
  elf = elf_headers.ElfHeader(data)
  for phdr in elf.GetPhdrs():
    if phdr.p_type == elf_headers.ProgramHeader.Type.PT_LOAD.value:
      # Current version of the prototype only supports ranges which are fully
      # contained inside one LOAD segment. It should cover most of the common
      # cases.
      if phdr.p_offset >= r or phdr.p_offset + phdr.p_filesz <= l:
        # Range doesn't overlap.
        continue
      if phdr.p_offset > l or phdr.p_offset + phdr.p_filesz < r:
        # Range overlap with LOAD segment but isn't fully covered by it.
        raise RuntimeError('Range is not contained within one LOAD segment')
      l_virt = phdr.p_vaddr + (l - phdr.p_offset)
      r_virt = phdr.p_vaddr + (r - phdr.p_offset)
      return l_virt, r_virt
  raise RuntimeError('Specified range is outside of all LOAD segments.')


def _CopyRangeIntoCompressedSection(data, l, r):
  """Adds a new section containing compressed version of provided range."""
  virtual_l, virtual_r = _FileRangeToVirtualAddressRange(data, l, r)
  # LOAD segments borders are being rounded to the page size so we have to
  # shrink [l, r) so corresponding virtual addresses are aligned.
  if virtual_l % ADDRESS_ALIGN != 0:
    l += ADDRESS_ALIGN - (virtual_l % ADDRESS_ALIGN)
  r -= virtual_r % ADDRESS_ALIGN
  if l >= r:
    raise RuntimeError('Range collapsed after aligning by page size')

  compressed_range = compression.CompressData(data[l:r])

  with tempfile.TemporaryDirectory() as tmpdir:
    # The easiest way to add a new section is to use objcopy, but it requires
    # for all of the data to be stored in files.
    objcopy_input_file = os.path.join(tmpdir, 'input')
    objcopy_data_file = os.path.join(tmpdir, 'data')
    objcopy_output_file = os.path.join(tmpdir, 'output')

    with open(objcopy_input_file, 'wb') as f:
      f.write(data)
    with open(objcopy_data_file, 'wb') as f:
      f.write(compressed_range)

    objcopy_args = [
        OBJCOPY_PATH, objcopy_input_file, objcopy_output_file, '--add-section',
        '{}={}'.format(COMPRESSED_SECTION_NAME, objcopy_data_file)
    ]
    run_result = subprocess.run(
        objcopy_args, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if run_result.returncode != 0:
      raise RuntimeError('objcopy failed with status code {}: {}'.format(
          run_result.returncode, run_result.stderr))

    with open(objcopy_output_file, 'rb') as f:
      data[:] = bytearray(f.read())


def main():
  _SetupLogging()
  args = _ParseArguments()

  with open(args.input, 'rb') as f:
    data = f.read()
    data = bytearray(data)

  _CopyRangeIntoCompressedSection(data, args.left_range, args.right_range)

  with open(args.output, 'wb') as f:
    f.write(data)
  return 0


if __name__ == '__main__':
  sys.exit(main())
