# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This library contains 64 bit ELF headers access and modification methods.

This library implements classes representing various ELF structures.
For more detailed description of ELF headers and fields refer to the ELF
standard: http://www.skyfree.org/linux/references/ELF_Format.pdf and to the
64 bit update: https://uclibc.org/docs/elf-64-gen.pdf.

This library was created because the script required precise manipulations of
file offsets. I.e: move the headers segment to the end of the file.
No package, capable of doing this kind of manipulations was found, so the
creation of this library was deemed necessary.

The point of entry is ElfHeader class that provides methods for accessing
additional parts of the ELF file.
"""

import enum
import logging


class ElfEntry(object):
  """Base class for ELF headers.

  Provides methods for populating fields.
  """

  def __init__(self, byte_order, fields=None):
    """ElfEntry constructor.

    Args:
      byte_order: str. Either 'little' for little endian or 'big' for big
        endian.
      fields: List[Tuple[str, int]]. An ordered list of pairs of
        (attribute name, size in bytes). This list will be used for parsing
        data and automatically setting up those fields.
    """
    if fields is None:
      self._fields = []
    else:
      self._fields = fields
    self.byte_order = byte_order

  def ParseBytes(self, data, offset):
    """Parses Entry fields from data starting at offset using _fields.

    Args:
        data: bytes.
        offset: int. The start point of parsing.
    """
    current_offset = offset
    for field_name, field_size in self._fields:
      value = int.from_bytes(
          data[current_offset:current_offset + field_size],
          byteorder=self.byte_order)
      setattr(self, field_name, value)
      current_offset += field_size

  def ApplyKwargs(self, **kwargs):
    """Set the fields from kwargs matching the _fields array entries."""
    for field_name, _ in self._fields:
      if field_name not in kwargs:
        logging.error('field_name %s not found in kwargs', field_name)
        continue
      setattr(self, field_name, kwargs[field_name])

  def ToBytes(self):
    """Returns byte representation of ELF entry."""
    bytearr = bytearray()
    for field_name, field_size in self._fields:
      field_bytes = getattr(self, field_name).to_bytes(
          field_size, byteorder=self.byte_order)
      bytearr.extend(field_bytes)
    return bytearr

  @classmethod
  def Create(cls, byte_order, **kwargs):
    """Static wrapper around ApplyKwargs method.

    Args:
      byte_order: str. Either 'little' for little endian or 'big' for big
        endian.
      **kwargs: will be passed directly to the ApplyKwargs method.
    """
    obj = cls(byte_order)
    obj.ApplyKwargs(**kwargs)
    return obj

  @classmethod
  def FromBytes(cls, byte_order, data, offset):
    """Static wrapper around ParseBytes method.

    Args:
      byte_order: str. Either 'little' for little endian or 'big' for big
        endian.
      data: bytes.
      offset: int. The start point of parsing.
    """
    obj = cls(byte_order)
    obj.ParseBytes(data, offset)
    return obj


class ProgramHeader(ElfEntry):
  """This class represent PhdrEntry from ELF standard."""

  class Type(enum.IntEnum):
    PT_NULL = 0
    PT_LOAD = 1
    PT_DYNAMIC = 2
    PT_INTERP = 3
    PT_NOTE = 4
    PT_SHLIB = 5
    PT_PHDR = 6

  class Flags(enum.IntFlag):
    PF_X = 1
    PF_W = 2
    PF_R = 4

  def __init__(self, byte_order):
    """ProgramHeader constructor.

    Args:
      byte_order: str.
    """
    # We have to set them here to avoid attribute-error from pylint.
    self.p_type = None
    self.p_flags = None
    self.p_offset = None
    self.p_vaddr = None
    self.p_paddr = None
    self.p_filesz = None
    self.p_memsz = None
    self.p_align = None
    fields = [
        ('p_type', 4),
        ('p_flags', 4),
        ('p_offset', 8),
        ('p_vaddr', 8),
        ('p_paddr', 8),
        ('p_filesz', 8),
        ('p_memsz', 8),
        ('p_align', 8),
    ]
    super(ProgramHeader, self).__init__(byte_order, fields)

  def FilePositionEnd(self):
    """Returns the end (exclusive) of the segment file range."""
    return self.p_offset + self.p_filesz


class ElfHeader(ElfEntry):
  """This class represents ELFHdr from the ELF standard.

  On its initialization it determines the bitness and endianness of the binary.
  """

  class EiClass(enum.IntEnum):
    ELFCLASS32 = 1
    ELFCLASS64 = 2

  class EiData(enum.IntEnum):
    ELFDATALSB = 1
    ELFDATAMSB = 2

  class EType(enum.IntEnum):
    ET_NONE = 0
    ET_REL = 1
    ET_EXEC = 2
    ET_DYN = 3
    ET_CORE = 4

  _EI_CLASS_OFFSET = 4
  _EI_DATA_OFFSET = 5

  def _GetEiClass(self, data):
    """Returns the value of ei_class."""
    return data[self._EI_CLASS_OFFSET]

  def _GetEiData(self, data):
    """Returns the value of ei_data."""
    return data[self._EI_DATA_OFFSET]

  def _ValidateBitness(self, data):
    """Verifies that library supports file's bitness."""
    if self._GetEiClass(data) != ElfHeader.EiClass.ELFCLASS64:
      raise RuntimeError('only 64 bit objects are supported')

  def _ReadByteOrder(self, data):
    """Reads and returns the file's byte order."""
    ei_data = data[self._EI_DATA_OFFSET]
    if ei_data == ElfHeader.EiData.ELFDATALSB:
      return 'little'
    elif ei_data == ElfHeader.EiData.ELFDATAMSB:
      return 'big'
    raise RuntimeError('Failed to parse ei_data')

  def _ParsePhdrs(self, data):
    current_offset = self.e_phoff
    for _ in range(0, self.e_phnum):
      self.phdrs.append(
          ProgramHeader.FromBytes(self.byte_order, data, current_offset))
      current_offset += self.e_phentsize

  def __init__(self, data):
    """ElfHeader constructor.

    Args:
      data: bytearray.
    """
    # We have to set them here to avoid attribute-error from pylint.
    self.ei_magic = None
    self.ei_class = None
    self.ei_data = None
    self.ei_version = None
    self.ei_osabi = None
    self.ei_abiversion = None
    self.ei_pad = None
    self.e_type = None
    self.e_machine = None
    self.e_version = None
    self.e_entry = None
    self.e_phoff = None
    self.e_shoff = None
    self.e_flags = None
    self.e_ehsize = None
    self.e_phentsize = None
    self.e_phnum = None
    self.e_shentsize = None
    self.e_shnum = None
    self.e_shstrndx = None
    fields = [
        ('ei_magic', 4),
        ('ei_class', 1),
        ('ei_data', 1),
        ('ei_version', 1),
        ('ei_osabi', 1),
        ('ei_abiversion', 1),
        ('ei_pad', 7),
        ('e_type', 2),
        ('e_machine', 2),
        ('e_version', 4),
        ('e_entry', 8),
        ('e_phoff', 8),
        ('e_shoff', 8),
        ('e_flags', 4),
        ('e_ehsize', 2),
        ('e_phentsize', 2),
        ('e_phnum', 2),
        ('e_shentsize', 2),
        ('e_shnum', 2),
        ('e_shstrndx', 2),
    ]

    self._ValidateBitness(data)
    byte_order = self._ReadByteOrder(data)
    super(ElfHeader, self).__init__(byte_order, fields)

    self.ParseBytes(data, 0)
    if self.e_type != ElfHeader.EType.ET_DYN:
      raise RuntimeError('Only shared libraries are supported')

    self.phdrs = []
    self._ParsePhdrs(data)

  def GetPhdrs(self):
    """Returns the list of file's program headers."""
    return self.phdrs

  def GetPhdrsByType(self, phdr_type):
    """Yields program headers of the given type."""
    return (phdr for phdr in self.phdrs if phdr.p_type == phdr_type)

  def AddPhdr(self, phdr):
    """Adds a new ProgramHeader entry correcting the e_phnum variable.

    This method will increase the size of LOAD segment containing the program
    headers without correcting the other offsets. It is up to the caller to
    deal with the results. One way to avoid any problems would be to move
    program headers to the end of the file.

    Args:
      phdr: ProgramHeader. Instance of ProgramHeader to add.
    """
    self.phdrs.append(phdr)

    phdrs_size = self.e_phnum * self.e_phentsize
    # We need to locate the LOAD segment containing program headers and
    # increase its size.
    phdr_found = False
    for phdr in self.GetPhdrsByType(ProgramHeader.Type.PT_LOAD):
      if phdr.p_offset > self.e_phoff:
        continue
      if phdr.FilePositionEnd() < self.e_phoff + phdrs_size:
        continue
      phdr.p_filesz += self.e_phentsize
      phdr.p_memsz += self.e_phentsize
      phdr_found = True
      break
    if not phdr_found:
      raise RuntimeError('Failed to increase program headers LOAD segment')

    # If PHDR segment exists it needs to be corrected as well.
    for phdr in self.GetPhdrsByType(ProgramHeader.Type.PT_PHDR):
      phdr.p_filesz += self.e_phentsize
      phdr.p_memsz += self.e_phentsize
    self.e_phnum += 1

  def _OrderPhdrs(self):
    """Orders program LOAD headers by p_vaddr to comply with standard."""

    def HeaderToKey(phdr):
      if phdr.p_type == ProgramHeader.Type.PT_LOAD:
        return (0, phdr.p_vaddr)
      else:
        # We want to preserve the order of non LOAD segments.
        return (1, 0)

    self.phdrs.sort(key=HeaderToKey)

  def PatchData(self, data):
    """Patches the given data array to reflect all changes made to the header.

    This method doesn't completely rewrite the data, instead it patches
    inplace. Not only the ElfHeader is patched but all of its ProgramHeader
    as well.

    The important limitation is that this method doesn't take changes of sizes
    and offsets into account. As example, if new ProgramHeader is added, this
    method will override whatever data is located under its placement so the
    user has to move the headers to the end beforehand or the user mustn't
    change header's size.

    Args:
      data: bytearray. The data array to be patched.
    """
    elf_bytes = self.ToBytes()
    data[:len(elf_bytes)] = elf_bytes
    current_offset = self.e_phoff
    self._OrderPhdrs()
    for phdr in self.GetPhdrs():
      phdr_bytes = phdr.ToBytes()
      data[current_offset:current_offset + len(phdr_bytes)] = phdr_bytes
      current_offset += self.e_phentsize
