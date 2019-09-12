#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import pathlib
import sys
import unittest

sys.path.append(str(pathlib.Path(__file__).resolve().parents[1]))
import elf_headers

ELF_HEADER_TEST_LIBRARY = 'testdata/lib.so'


class ElfHeaderTest(unittest.TestCase):

  def setUp(self):
    super(ElfHeaderTest, self).setUp()
    script_dir = os.path.dirname(os.path.abspath(__file__))
    self.library_path = os.path.join(script_dir, ELF_HEADER_TEST_LIBRARY)

  def testElfHeaderParsing(self):
    with open(self.library_path, 'rb') as f:
      data = f.read()
    elf = elf_headers.ElfHeader(data)
    # Validating all of the ELF header fields.
    self.assertEqual(elf.e_type, elf_headers.ElfHeader.EType.ET_DYN)
    self.assertEqual(elf.e_entry, 0x1040)
    self.assertEqual(elf.e_phoff, 64)
    self.assertEqual(elf.e_shoff, 14136)
    self.assertEqual(elf.e_flags, 0)
    self.assertEqual(elf.e_ehsize, 64)
    self.assertEqual(elf.e_phentsize, 56)
    self.assertEqual(elf.e_phnum, 8)
    self.assertEqual(elf.e_shentsize, 64)
    self.assertEqual(elf.e_shnum, 26)
    self.assertEqual(elf.e_shstrndx, 25)
    # Validating types and amounts of all segments excluding GNU specific ones.
    phdrs = elf.GetPhdrs()
    self.assertEqual(len(phdrs), 8)

    phdr_types = [
        elf_headers.ProgramHeader.Type.PT_LOAD,
        elf_headers.ProgramHeader.Type.PT_LOAD,
        elf_headers.ProgramHeader.Type.PT_LOAD,
        elf_headers.ProgramHeader.Type.PT_LOAD,
        elf_headers.ProgramHeader.Type.PT_DYNAMIC,
        None,
        None,
        None,
    ]
    for i in range(0, len(phdrs)):
      if phdr_types[i] is not None:
        self.assertEqual(phdrs[i].p_type, phdr_types[i])

    # Validating all of the fields of the first segment.
    load_phdr = phdrs[0]
    self.assertEqual(load_phdr.p_offset, 0x0)
    self.assertEqual(load_phdr.p_vaddr, 0x0)
    self.assertEqual(load_phdr.p_paddr, 0x0)
    self.assertEqual(load_phdr.p_filesz, 0x468)
    self.assertEqual(load_phdr.p_memsz, 0x468)
    self.assertEqual(load_phdr.p_flags, 0b100)
    self.assertEqual(load_phdr.p_align, 0x1000)
    # Validating offsets of the second segment
    load_phdr = phdrs[1]
    self.assertEqual(load_phdr.p_offset, 0x1000)
    self.assertEqual(load_phdr.p_vaddr, 0x1000)
    self.assertEqual(load_phdr.p_paddr, 0x1000)

  def testElfHeaderNoopPatching(self):
    """Patching the ELF without any changes."""
    with open(self.library_path, 'rb') as f:
      data = bytearray(f.read())
    data_copy = data[:]
    elf = elf_headers.ElfHeader(data)
    elf.PatchData(data)
    self.assertEqual(data, data_copy)

  def testElfHeaderPatchingAndParsing(self):
    """Patching the ELF and validating that it worked."""
    with open(self.library_path, 'rb') as f:
      data = bytearray(f.read())
    elf = elf_headers.ElfHeader(data)
    # Changing some values.
    elf.e_ehsize = 42
    elf.GetPhdrs()[0].p_align = 1
    elf.GetPhdrs()[0].p_filesz = 10
    elf.PatchData(data)

    updated_elf = elf_headers.ElfHeader(data)
    # Validating all of the ELF header fields.
    self.assertEqual(updated_elf.e_type, elf.e_type)
    self.assertEqual(updated_elf.e_entry, elf.e_entry)
    self.assertEqual(updated_elf.e_phoff, elf.e_phoff)
    self.assertEqual(updated_elf.e_shoff, elf.e_shoff)
    self.assertEqual(updated_elf.e_flags, elf.e_flags)
    self.assertEqual(updated_elf.e_ehsize, 42)
    self.assertEqual(updated_elf.e_phentsize, elf.e_phentsize)
    self.assertEqual(updated_elf.e_phnum, elf.e_phnum)
    self.assertEqual(updated_elf.e_shentsize, elf.e_shentsize)
    self.assertEqual(updated_elf.e_shnum, elf.e_shnum)
    self.assertEqual(updated_elf.e_shstrndx, elf.e_shstrndx)

    # Validating all of the fields of the first segment.
    load_phdr = elf.GetPhdrs()[0]
    updated_load_phdr = updated_elf.GetPhdrs()[0]

    self.assertEqual(updated_load_phdr.p_offset, load_phdr.p_offset)
    self.assertEqual(updated_load_phdr.p_vaddr, load_phdr.p_vaddr)
    self.assertEqual(updated_load_phdr.p_paddr, load_phdr.p_paddr)
    self.assertEqual(updated_load_phdr.p_filesz, 10)
    self.assertEqual(updated_load_phdr.p_memsz, load_phdr.p_memsz)
    self.assertEqual(updated_load_phdr.p_flags, load_phdr.p_flags)
    self.assertEqual(updated_load_phdr.p_align, 1)


if __name__ == '__main__':
  unittest.main()
