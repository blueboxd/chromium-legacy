#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
import zipfile

sys.path.insert(
    1,
    os.path.join(
        os.path.dirname(__file__), '..', '..', '..', '..', 'build', 'android'))
import devil_chromium
from pylib import constants

sys.path.insert(1,
                constants.host_paths.ANDROID_PLATFORM_DEVELOPMENT_SCRIPTS_PATH)
import stack

_ZIPALIGN_PATH = os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'android_sdk', 'public',
    'build-tools', constants.ANDROID_SDK_BUILD_TOOLS_VERSION, 'zipalign')

# These tests exercise stack.py by generating fake APKs (zip-aligned archives),
# full of fake .so files (with ELF headers), and using a fake symbolizer.
#
# The symbolizer returns deterministic function descriptions given an address
# and library name, so that test cases can be easily contrived. Eg:
#
#   libchrome.so at 0x174 --> chrome::Func_174 at chrome.cc:1:1
#
# All libraries generated are slightly under 4K in size (0x1000). This means
# that when a fake APK is generated, libraries within it will reside at
# consecutive 4K boundaries. Eg., an APK with libfoo.so and libbar.so:
#
#   libfoo.so will reside at APK offset 0x1000
#   libbar.so will reside at APK offset 0x2000
#
# Each test invokes stack.py with a given test input (fudged trace lines), runs
# them through stack.py with the fake symbolizer, grabs output, and matches it
# line-for-line with the expected output. Whitespace is ignored at that step, so
# that test expectations don't have to be column-accurate.


class FakeSymbolizer:

  def __init__(self, directory):
    self._lib_directory = directory

  def GetSymbolInformation(self, file, address_string):
    basename = os.path.basename(file)
    local_file = os.path.join(self._lib_directory, basename)

    # If the library doesn't exist, the LLVM symbolizer wrapper script
    # intercepts the call and returns <UNKNOWN>.
    if not os.path.exists(local_file):
      return [('<UNKNOWN>', file)]

    # If the address isn't in the library, LLVM symbolizer yields ??.
    lib_size = os.stat(local_file).st_size
    address = int(address_string, 16)
    if address >= lib_size:
      return [('??', '??:0:0')]

    namespace = basename.split('.')[0].replace('lib', '', 1)
    method_name = '{}::Func_{:X}'.format(namespace, address)
    return [(method_name, '{}.cc:1:1'.format(namespace))]


class StackDecodeTest(unittest.TestCase):

  def setUp(self):
    self._temp_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self._temp_dir)

  def _MakeElf(self, file):
    # Create a library slightly less than 4K in size, so that when added to an
    # APK archive, all libraries end up on 4K boundaries.
    data = '\x7fELF' + ' ' * 0xE00
    with open(file, 'wb') as f:
      f.write(data)

  # Build a dummy APK with native libraries in it.
  def _MakeApk(self, apk, libs, apk_dir, out_dir):
    unaligned_apk = os.path.join(self._temp_dir, 'unaligned_apk')
    with zipfile.ZipFile(unaligned_apk, 'w') as archive:
      for lib in libs:
        # Make an ELF-format .so file. The fake symbolizer will fudge functions
        # for libraries that exist.
        library_file = os.path.join(out_dir, lib)
        self._MakeElf(library_file)

        # Add the library to the APK.
        archive.write(library_file, lib, compress_type=None)

    # Zip-align the APK so library offsets are round numbers.
    apk_file = os.path.join(apk_dir, apk)
    subprocess.check_output(
        [_ZIPALIGN_PATH, '-p', '-f', '4', unaligned_apk, apk_file])

  # Accept either a multi-line string or a list of strings, strip leading and
  # trailing whitespace, and return the strings as a list.
  def _StripLines(self, text):
    if isinstance(text, str):
      lines = text.splitlines()
    else:
      assert isinstance(text, list)
      lines = text
    lines = [line.strip() for line in lines]
    return [line for line in lines if line]

  def _RunCase(self, logcat, expected, apks):
    # Set up staging directories.
    temp = self._temp_dir
    out_dir = os.path.join(temp, 'out', 'Debug')
    os.makedirs(out_dir)
    apk_dir = os.path.join(out_dir, 'apks')
    os.makedirs(apk_dir)

    input_file = os.path.join(temp, 'input.txt')
    output_file = os.path.join(temp, 'output.txt')

    # Create test APKs, with .so libraries in them, that are real enough to
    # trick the stack decoder.
    for name, libs in apks.items():
      self._MakeApk(name, libs, apk_dir, out_dir)

    symbolizer = FakeSymbolizer(out_dir)

    # Put the input into a temp file.
    with open(input_file, 'w') as f:
      input_lines = self._StripLines(logcat)
      f.write('\n'.join(input_lines))

    # Run the stack script and capture its stdout in a file.
    # TODO(cjgrant): Figure out how to output to a stream buffer instead.
    stack_script_args = [
        '--output-directory',
        out_dir,
        '--apks-directory',
        apk_dir,
        input_file,
    ]
    with open(output_file, 'w') as f:
      old_stdout = sys.stdout
      sys.stdout = f
      try:
        stack.main(stack_script_args, test_symbolizer=symbolizer)
      except:
        pass
      sys.stdout.flush()
      sys.stdout = old_stdout

    # Filter out all output lines before actual decoding starts.
    with open(output_file, 'r') as f:
      lines = f.readlines()
      delimiter = [l for l in lines if 'RELADDR' in l]
      if delimiter:
        index = lines.index(delimiter[-1])
        output_lines = lines[index + 1:]
        output_lines = self._StripLines(output_lines)
      else:
        output_lines = []

    # Tokenize the input and output so that we can ignore whitespace in the
    # validation. This way a test doesn't fail if a column shifts slightly.
    expected_lines = self._StripLines(expected)
    expected_tokens = [line.split() for line in expected_lines]
    actual_tokens = [line.split() for line in output_lines]

    self.assertEqual(len(expected_tokens), len(actual_tokens))
    for i in xrange(len(expected_tokens)):
      self.assertEqual(expected_tokens[i], actual_tokens[i])

  def test_BasicDecoding(self):
    apks = {
        'chrome.apk': ['libchrome.so', 'libfoo.so'],
    }
    input = textwrap.dedent('''
      DEBUG : #01 pc 00000174  /path==/base.apk (offset 0x00001000)
      DEBUG : #02 pc 00000274  /path==/base.apk (offset 0x00002000)
      DEBUG : #03 pc 00000374  /path==/lib/arm/libchrome.so
      ''')
    expected = textwrap.dedent('''
      00000174   chrome::Func_174         chrome.cc:1:1
      00000274   foo::Func_274            foo.cc:1:1
      00000374   chrome::Func_374         chrome.cc:1:1
      ''')
    self._RunCase(input, expected, apks)

  def test_OutOfRangeAddresses(self):
    apks = {
        'chrome.apk': ['libchrome.so'],
    }
    # Test offsets where the address is outside the range of a valid library,
    # and when the offset does not correspond to a valid library.
    input = textwrap.dedent('''
      DEBUG : #01 pc 00777777  /path==/base.apk (offset 0x00001000)
      DEBUG : #02 pc 00000374  /path==/base.apk (offset 0x00003000)
      ''')
    expected = textwrap.dedent('''
      00777777   ??                       ??:0:0
      00000374   offset 0x00003000        /path==/base.apk
      ''')
    self._RunCase(input, expected, apks)

  def test_SystemLibraries(self):
    apks = {
        'chrome.apk': [],
    }
    # Here, the frames are in an on-device system library. If the trace is able
    # to supply a symbol name, ensure it's preserved in the output.
    input = textwrap.dedent('''
      DEBUG : #01 pc 00000474  /system/lib/libart.so (art_function+40)
      DEBUG : #02 pc 00000474  /system/lib/libart.so
      ''')
    expected = textwrap.dedent('''
      00000474   art_function+40          /system/lib/libart.so
      00000474   <UNKNOWN>                /system/lib/libart.so
      ''')
    self._RunCase(input, expected, apks)


if __name__ == '__main__':
  unittest.main()
