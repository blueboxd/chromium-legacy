#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import os.path
import shutil
import tempfile
import unittest

import mojom_parser

from mojom.generate import module


class MojomParserTest(unittest.TestCase):
  """Tests covering the behavior defined by the main mojom_parser.py script.
  This includes behavior around input and output path manipulation, dependency
  resolution, and module serialization and deserialization."""

  def __init__(self, method_name):
    super(MojomParserTest, self).__init__(method_name)
    self._temp_dir = None

  def setUp(self):
    self._temp_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self._temp_dir)
    self._temp_dir = None

  def GetPath(self, path):
    assert not os.path.isabs(path)
    return os.path.join(self._temp_dir, path)

  def GetModulePath(self, path):
    assert not os.path.isabs(path)
    return os.path.join(self.GetPath('out'), path) + '-module'

  def WriteFile(self, path, contents):
    full_path = self.GetPath(path)
    dirname = os.path.dirname(full_path)
    if not os.path.exists(dirname):
      os.makedirs(dirname)
    with open(full_path, 'w') as f:
      f.write(contents)

  def LoadModule(self, mojom_path):
    with open(self.GetModulePath(mojom_path), 'rb') as f:
      return module.Module.Load(f)

  def ParseMojoms(self, mojoms):
    """Parse all input mojoms relative the temp dir."""
    out_dir = self.GetPath('out')
    mojom_parser.ParseMojoms(
        map(lambda mojom: os.path.join(self._temp_dir, mojom), mojoms),
        [self._temp_dir, out_dir], out_dir, [])

  def testBasicParse(self):
    """Basic test to verify that we can parse a mojom file and get a module."""
    mojom = 'foo/bar.mojom'
    self.WriteFile(
        mojom, """\
        module test;
        enum TestEnum { kFoo };
        """)
    self.ParseMojoms([mojom])

    m = self.LoadModule(mojom)
    self.assertEqual('foo/bar.mojom', m.path)
    self.assertEqual('test', m.mojom_namespace)
    self.assertEqual(1, len(m.enums))

  def testBasicParseWithAbsolutePaths(self):
    """Verifies that we can parse a mojom file given an absolute path input."""
    mojom = 'foo/bar.mojom'
    self.WriteFile(
        mojom, """\
        module test;
        enum TestEnum { kFoo };
        """)
    self.ParseMojoms([self.GetPath(mojom)])

    m = self.LoadModule(mojom)
    self.assertEqual('foo/bar.mojom', m.path)
    self.assertEqual('test', m.mojom_namespace)
    self.assertEqual(1, len(m.enums))

  def testImport(self):
    """Verify imports within the same set of mojom inputs."""
    a = 'a.mojom'
    b = 'b.mojom'
    self.WriteFile(
        a, """\
        module a;
        import "b.mojom";
        struct Foo { b.Bar bar; };""")
    self.WriteFile(b, """\
        module b;
        struct Bar {};""")
    self.ParseMojoms([a, b])

    ma = self.LoadModule(a)
    mb = self.LoadModule(b)
    self.assertEqual('a.mojom', ma.path)
    self.assertEqual('b.mojom', mb.path)
    self.assertEqual(1, len(ma.imports))
    self.assertEqual(mb, ma.imports[0])

  def testPreProcessedImport(self):
    """Verify imports processed by a previous parser execution can be loaded
    properly when parsing a dependent mojom."""
    a = 'a.mojom'
    self.WriteFile(a, """\
        module a;
        struct Bar {};""")
    self.ParseMojoms([a])

    b = 'b.mojom'
    self.WriteFile(
        b, """\
        module b;
        import "a.mojom";
        struct Foo { a.Bar bar; };""")
    self.ParseMojoms([b])

  def testMissingImport(self):
    """Verify that an import fails if the imported mojom does not exist."""
    a = 'a.mojom'
    self.WriteFile(
        a, """\
        module a;
        import "non-existent.mojom";
        struct Bar {};""")
    with self.assertRaisesRegexp(ValueError, "does not exist"):
      self.ParseMojoms([a])

  def testUnparsedImport(self):
    """Verify that an import fails if the imported mojom is not in the set of
    mojoms provided to the parser on this execution AND there is no pre-existing
    parsed output module already on disk for it."""
    a = 'a.mojom'
    b = 'b.mojom'
    self.WriteFile(a, """\
        module a;
        struct Bar {};""")
    self.WriteFile(
        b, """\
        module b;
        import "a.mojom";
        struct Foo { a.Bar bar; };""")

    # a.mojom has not been parsed yet, so its import will fail when processing
    # b.mojom here.
    with self.assertRaisesRegexp(ValueError, "does not exist"):
      self.ParseMojoms([b])
