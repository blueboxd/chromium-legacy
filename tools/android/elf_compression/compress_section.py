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


def AlignUp(addr, page_size=ADDRESS_ALIGN):
  """Rounds up given address to be aligned to page_size.

  Args:
    addr: int. Virtual address to be aligned.
    page_size: int. Page size to be used for the alignment.
  """
  if addr % page_size != 0:
    addr += page_size - (addr % page_size)
  return addr


def AlignDown(addr, page_size=ADDRESS_ALIGN):
  """Round down given address to be aligned to page_size.

  Args:
    addr: int. Virtual address to be aligned.
    page_size: int. Page size to be used for the alignment.
  """
  return addr - addr % page_size


def MatchVaddrAlignment(vaddr, offset, align=ADDRESS_ALIGN):
  """Align vaddr to comply with ELF standard binary alignment.

  Increases vaddr until the following is true:
    vaddr % align == offset % align

  Args:
    vaddr: virtual address to be aligned.
    offset: file offset to be aligned.
    align: alignment value.

  Returns:
    Aligned virtual address, bigger or equal than the vaddr.
  """
  delta = offset % align - vaddr % align
  if delta < 0:
    delta += align
  return vaddr + delta


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
    if phdr.p_type == elf_headers.ProgramHeader.Type.PT_LOAD:
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
  l += AlignUp(virtual_l) - virtual_l
  r -= virtual_r - AlignDown(virtual_r)
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


def _FindNewVaddr(phdrs):
  """Returns the virt address that is safe to use for insertion of new data."""
  max_vaddr = 0
  # Strictly speaking it should be sufficient to look through only LOAD
  # segments, but better be safe than sorry.
  for phdr in phdrs:
    max_vaddr = max(max_vaddr, phdr.p_vaddr + phdr.p_memsz)
  # When the mapping occurs end address is increased to be a multiple
  # of page size. To ensure compatibility with anything relying on this
  # behaviour we take this increase into account.
  max_vaddr = AlignUp(max_vaddr)
  return max_vaddr


def _MovePhdrToTheEnd(data):
  """Moves Phdrs to the end of the file and adjusts all references to it."""
  elf_hdr = elf_headers.ElfHeader(data)

  # If program headers are already in the end of the file, nothing to do.
  if elf_hdr.e_phoff + elf_hdr.e_phnum * elf_hdr.e_phentsize == len(data):
    return

  new_phoff = len(data)
  unaligned_new_vaddr = _FindNewVaddr(elf_hdr.GetPhdrs())
  new_vaddr = MatchVaddrAlignment(unaligned_new_vaddr, new_phoff)
  # Since we moved the PHDR section to the end of the file, we need to create a
  # new LOAD segment to load it in.
  new_filesz = (elf_hdr.e_phnum + 1) * elf_hdr.e_phentsize
  elf_hdr.AddPhdr(
      elf_headers.ProgramHeader.Create(
          elf_hdr.byte_order,
          p_type=elf_headers.ProgramHeader.Type.PT_LOAD,
          p_flags=elf_headers.ProgramHeader.Flags.PF_R,
          p_offset=new_phoff,
          p_vaddr=new_vaddr,
          p_paddr=new_vaddr,
          p_filesz=new_filesz,
          p_memsz=new_filesz,
          p_align=ADDRESS_ALIGN,
      ))

  # PHDR segment if it exists should point to the new location.
  for phdr in elf_hdr.GetPhdrs():
    if phdr.p_type == elf_headers.ProgramHeader.Type.PT_PHDR:
      phdr.p_offset = new_phoff
      phdr.p_vaddr = new_vaddr
      phdr.p_paddr = new_vaddr
      phdr.p_filesz = new_filesz
      phdr.p_memsz = new_filesz
      phdr.p_align = ADDRESS_ALIGN

  # We need to replace the previous phdr placement with zero bytes to fail
  # fast if dynamic linker doesn't like the new program header.
  previous_phdr_size = (elf_hdr.e_phnum - 1) * elf_hdr.e_phentsize
  data[elf_hdr.e_phoff:elf_hdr.e_phoff +
       previous_phdr_size] = [0] * previous_phdr_size

  # Updating ELF header to point to the new location.
  elf_hdr.e_phoff = new_phoff
  elf_hdr.PatchData(data)


def main():
  _SetupLogging()
  args = _ParseArguments()

  with open(args.input, 'rb') as f:
    data = f.read()
    data = bytearray(data)

  _CopyRangeIntoCompressedSection(data, args.left_range, args.right_range)
  _MovePhdrToTheEnd(data)

  with open(args.output, 'wb') as f:
    f.write(data)
  return 0


if __name__ == '__main__':
  sys.exit(main())
