#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import polymer
import os
import shutil
import tempfile
import unittest


_HERE_DIR = os.path.dirname(__file__)


class PolymerModulizerTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None

  def tearDown(self):
    shutil.rmtree(self._out_folder)

  def _read_out_file(self, file_name):
    assert self._out_folder
    return open(os.path.join(self._out_folder, file_name), 'r').read()

  def _run_test(self, html_type, html_file, js_file,
      js_out_file, js_file_expected):
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    polymer.main([
      '--in_folder', 'tests',
      '--out_folder', self._out_folder,
      '--js_file',  js_file,
      '--html_file',  html_file,
      '--html_type',  html_type,
      '--namespace_rewrites',  'Polymer.PaperRippleBehavior|PaperRippleBehavior',
    ])

    actual_js = self._read_out_file(js_out_file)
    expected_js = open(os.path.join('tests', js_file_expected), 'r').read()
    self.assertEquals(expected_js, actual_js)

  # Test case where HTML is extracted from a Polymer2 <dom-module>.
  def testSuccess_DomModule(self):
    self._run_test(
        'dom-module', 'dom_module.html', 'dom_module.js',
        'dom_module.m.js', 'dom_module_expected.js')

  # Test case where HTML is extracted from a Polymer2 style module.
  def testSuccess_StyleModule(self):
    self._run_test(
        'style-module', 'style_module.html', 'style_module.m.js',
        'style_module.m.js', 'style_module_expected.js')
    return

  # Test case where HTML is extracted from a Polymer2 <custom-style>.
  def testSuccess_CustomStyle(self):
    self._run_test(
        'custom-style', 'custom_style.html', 'custom_style.m.js',
        'custom_style.m.js', 'custom_style_expected.js')

  # Test case where HTML is extracted from a Polymer2 iron-iconset-svg file.
  def testSuccess_IronIconset(self):
    self._run_test(
        'iron-iconset', 'iron_iconset.html', 'iron_iconset.m.js',
        'iron_iconset.m.js', 'iron_iconset_expected.js')

  # Test case where the provided HTML is already in the form needed by Polymer3.
  def testSuccess_V3Ready(self):
    self._run_test(
        'v3-ready', 'v3_ready.html', 'v3_ready.js',
        'v3_ready.js', 'v3_ready_expected.js')


if __name__ == '__main__':
  unittest.main()
