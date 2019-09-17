#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for the compress_section script."""

import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

sys.path.append(str(pathlib.Path(__file__).resolve().parents[1]))
import compress_section

LIBRARY_CC_NAME = 'libtest.cc'
OPENER_CC_NAME = 'library_opener.cc'

SCRIPT_PATH = '../compress_section.py'

# src/third_party/llvm-build/Release+Asserts/bin/clang++
LLVM_CLANG_PATH = pathlib.Path(__file__).resolve().parents[4].joinpath(
    'third_party/llvm-build/Release+Asserts/bin/clang++')

# The array that we are trying to cut out of the file have those bytes at
# its start and end. This is done to simplify the test code to not perform
# full parse of the library to resolve the symbol.
MAGIC_BEGIN = bytes([151, 155, 125, 68])
MAGIC_END = bytes([236, 55, 136, 224])


class CompressionScriptTest(unittest.TestCase):
  # Error output of the script could be large enough to be trimmed by the
  # default setting, so disabling trimming on assertEqual.
  maxDiff = None

  def setUp(self):
    super(CompressionScriptTest, self).setUp()
    self.tmpdir_object = tempfile.TemporaryDirectory()
    self.tmpdir = self.tmpdir_object.name

    script_dir = os.path.dirname(os.path.abspath(__file__))
    self.library_cc_path = os.path.join(script_dir, LIBRARY_CC_NAME)
    self.opener_cc_path = os.path.join(script_dir, OPENER_CC_NAME)
    self.script_path = os.path.join(script_dir, SCRIPT_PATH)

  def tearDown(self):
    self.tmpdir_object.cleanup()
    super(CompressionScriptTest, self).tearDown()

  def _FindArrayRange(self, library_path):
    with open(library_path, 'rb') as f:
      data = f.read()
    l = data.find(MAGIC_BEGIN)
    r = data.find(MAGIC_END) + len(MAGIC_END)
    return l, r

  def _BuildLibrary(self):
    library_path = os.path.join(self.tmpdir, 'libtest.so')
    library_build_result = subprocess.run([
        LLVM_CLANG_PATH, '-shared', '-fPIC', '-O2', self.library_cc_path, '-o',
        library_path
    ])
    self.assertEqual(library_build_result.returncode, 0)
    return library_path

  def _BuildOpener(self):
    opener_path = os.path.join(self.tmpdir, 'library_opener')
    opener_build_result = subprocess.run([
        LLVM_CLANG_PATH, '-O2', self.opener_cc_path, '-o', opener_path, '-ldl'
    ])
    self.assertEqual(opener_build_result.returncode, 0)
    return opener_path

  def _RunScript(self, library_path):
    # Finding array borders.
    l, r = self._FindArrayRange(library_path)
    self.assertNotEqual(l, -1)
    self.assertLessEqual(l, r)

    output_path = os.path.join(self.tmpdir, 'patchedlibtest.so')
    script_arguments = [
        self.script_path,
        '-i',
        library_path,
        '-o',
        output_path,
        '-l',
        str(l),
        '-r',
        str(r),
    ]
    script_run_result = subprocess.run(
        script_arguments,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding='utf-8')
    self.assertEqual(script_run_result.stderr, '')
    self.assertEqual(script_run_result.returncode, 0)
    return output_path

  def _RunOpener(self, opener_path, patched_library_path):
    opener_run_result = subprocess.run([opener_path, patched_library_path],
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE,
                                       encoding='utf-8')

    self.assertEqual(opener_run_result.stderr, '')
    self.assertEqual(opener_run_result.returncode, 0)
    return opener_run_result.stdout

  def testLibraryPatching(self):
    """Runs the script on a test library and validates that it still works."""
    library_path = self._BuildLibrary()
    opener_path = self._BuildOpener()

    patched_library_path = self._RunScript(library_path)
    opener_output = self._RunOpener(opener_path, patched_library_path)
    self.assertEqual(opener_output, '4096\n')

  def testAlignUp(self):
    """Tests for AlignUp method of the script."""
    self.assertEqual(compress_section.AlignUp(1024, 1024), 1024)
    self.assertEqual(compress_section.AlignUp(1023, 1024), 1024)
    self.assertEqual(compress_section.AlignUp(1025, 1024), 2048)
    self.assertEqual(compress_section.AlignUp(5555, 4096), 8192)

  def testAlignDown(self):
    """Tests for AlignDown method of the script."""
    self.assertEqual(compress_section.AlignDown(1024, 1024), 1024)
    self.assertEqual(compress_section.AlignDown(1023, 1024), 0)
    self.assertEqual(compress_section.AlignDown(1025, 1024), 1024)
    self.assertEqual(compress_section.AlignDown(5555, 4096), 4096)

  def testMatchVaddrAlignment(self):
    """Tests for MatchVaddrAlignment method of the script."""
    self.assertEqual(compress_section.MatchVaddrAlignment(100, 100, 1024), 100)
    self.assertEqual(compress_section.MatchVaddrAlignment(99, 100, 1024), 100)
    self.assertEqual(compress_section.MatchVaddrAlignment(101, 100, 1024), 1124)
    self.assertEqual(
        compress_section.MatchVaddrAlignment(1024, 2049, 1024), 1025)


if __name__ == '__main__':
  unittest.main()
