#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unpack_pak
import unittest


class UnpackPakTest(unittest.TestCase):
  def testMapFileLine(self):
    self.assertTrue(unpack_pak.ParseLine('  {"path.js", IDR_PATH}'))

  def testGzippedMapFileLine(self):
    self.assertTrue(unpack_pak.ParseLine('  {"path.js", IDR_PATH, false}'))
    self.assertTrue(unpack_pak.ParseLine('  {"path.js", IDR_PATH, true}'))

  def testGetFileAndDirName(self):
    (f, d) = unpack_pak.GetFileAndDirName('out/build/gen/foo', 'a/b.js')
    self.assertEquals('b.js', f)
    self.assertEquals('a', d)

  def testGetFileAndDirNameForGeneratedResource(self):
    (f, d) = unpack_pak.GetFileAndDirName(
        'out/build/gen/foo',
        '@out_folder@/out/build/gen/foo/a/b.js')
    self.assertEquals('b.js', f)
    self.assertEquals('a', d)

if __name__ == '__main__':
  unittest.main()
