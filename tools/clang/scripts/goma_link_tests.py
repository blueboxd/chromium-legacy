#! /usr/bin/env python
# Copyright (c) 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Unit tests for goma_link.
#
# Usage:
#
# Ensure that gomacc, llvm-objdump, and llvm-dwarfdump are in your PATH.
# Then run:
#
# python third_party/pycoverage run tools/clang/scripts/goma_link_tests.py
#
# An HTML coverage report can be generated afterwards by running:
# python third_party/pycoverage html
#
# The report will be available as htmlcov/index.html

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import goma_ld
import goma_link

import os
import re
import shutil
import subprocess
import tempfile
import unittest

# Path constants.
CHROMIUM_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
LLVM_BIN_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'llvm-build',
                            'Release+Asserts', 'bin')


def _create_inputs(path):
  """
  Creates input files under path.
  """
  with open(os.path.join(path, 'main.cpp'), 'w') as f:
    f.write('extern int foo();\n'
            'int main(int argc, char *argv[]) {\n  return foo();\n}\n')
  with open(os.path.join(path, 'foo.cpp'), 'w') as f:
    f.write('int foo() {\n  return 12;\n}\n')
  with open(os.path.join(path, 'bar.cpp'), 'w') as f:
    f.write('int bar() {\n  return 9;\n}\n')


# tempfile.NamedDirectory is in Python 3.8. This is for compatibility with
# older Python versions.
class NamedDirectory(object):
  def __init__(self, *args, **kwargs):
    self.name = tempfile.mkdtemp(*args, **kwargs)

  def __enter__(self):
    return self.name

  def __exit__(self, exnty, *args, **kwargs):
    shutil.rmtree(self.name)
    return exnty is None


# Changes working directory to the specified directory, runs enclosed code,
# and changes back to the previous directory.
class WorkingDirectory(object):
  def __init__(self, newcwd):
    self.oldcwd = os.getcwd()
    os.chdir(newcwd)
    self.newcwd = os.getcwd()

  def __enter__(self):
    return self.newcwd

  def __exit__(self, exnty, *args, **kwargs):
    os.chdir(self.oldcwd)
    return exnty is None


class GomaLinkUnixWhitelistMain(goma_ld.GomaLinkUnix):
  """
  Same as goma_ld.GomaLinkUnix, but whitelists "main".
  """

  def __init__(self, *args, **kwargs):
    super(GomaLinkUnixWhitelistMain, self).__init__(*args, **kwargs)
    self.WHITELISTED_TARGETS = {'main'}


class GomaLinkWindowsWhitelistMain(goma_link.GomaLinkWindows):
  """
  Same as goma_ld.GomaLinkWindows, but whitelists "main".
  """

  def __init__(self, *args, **kwargs):
    super(GomaLinkWindowsWhitelistMain, self).__init__(*args, **kwargs)
    self.WHITELISTED_TARGETS = {'main.exe'}


class GomaLinkUnitTest(unittest.TestCase):
  """
  Unit tests for goma_link.
  """

  def test_ensure_file_no_dir(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      self.assertFalse(os.path.exists('test'))
      goma_link.ensure_file('test')
      self.assertTrue(os.path.exists('test'))

  def test_ensure_file_existing(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      self.assertFalse(os.path.exists('foo/test'))
      goma_link.ensure_file('foo/test')
      self.assertTrue(os.path.exists('foo/test'))
      os.utime('foo/test', (0, 0))
      statresult = os.stat('foo/test')
      goma_link.ensure_file('foo/test')
      self.assertTrue(os.path.exists('foo/test'))
      newstatresult = os.stat('foo/test')
      self.assertEqual(newstatresult.st_mtime, statresult.st_mtime)

  def test_ensure_file_error(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      self.assertFalse(os.path.exists('test'))
      goma_link.ensure_file('test')
      self.assertTrue(os.path.exists('test'))
      self.assertRaises(OSError, goma_link.ensure_file, 'test/impossible')


class GomaLinkIntegrationTest(unittest.TestCase):
  def clangcl(self):
    return os.path.join(LLVM_BIN_DIR, 'clang-cl' + goma_link.exe_suffix())

  def lld_link(self):
    return os.path.join(LLVM_BIN_DIR, 'lld-link' + goma_link.exe_suffix())

  def test_distributed_lto_common_objs(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      _create_inputs(d)
      os.makedirs('obj')
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', 'main.cpp',
          '-Foobj/main.obj'
      ])
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', 'foo.cpp', '-Foobj/foo.obj'
      ])
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', 'bar.cpp', '-Foobj/bar.obj'
      ])
      subprocess.check_call(
          ['llvm-ar', 'crsT', 'obj/foobar.lib', 'obj/bar.obj', 'obj/foo.obj'])
      with open('main.rsp', 'w') as f:
        f.write('obj/main.obj\n' 'obj/foobar.lib\n')
      with open('my_goma.sh', 'w') as f:
        f.write('#! /bin/sh\n\ngomacc "$@"\n')
      os.chmod('my_goma.sh', 0o755)
      rc = goma_link.GomaLinkWindows().main([
          'goma_link.py', '--gomacc', './my_goma.sh', '--',
          self.lld_link(), '-nodefaultlib', '-entry:main', '-out:main.exe',
          '@main.rsp'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # Check codegen parameters.
      with open(os.path.join(d, 'lto.main.exe', 'build.ninja')) as f:
        buildrules = f.read()
        codegen_match = re.search('^rule codegen\\b.*?^[^ ]', buildrules,
                                  re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(codegen_match)
        codegen_text = codegen_match.group(0)
        self.assertIn('my_goma.sh', codegen_text)
        self.assertNotIn('-flto', codegen_text)
        self.assertIn('build common_objs/obj/main.obj : codegen ', buildrules)
        self.assertIn('build common_objs/obj/foo.obj : codegen ', buildrules)
        self.assertIn(' index = common_objs/empty.thinlto.bc', buildrules)
        link_match = re.search('^build main.exe : native-link\\b.*?^[^ ]',
                               buildrules, re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(link_match)
        link_text = link_match.group(0)
        self.assertNotIn('main.exe.split.obj', link_text)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main.exe'])
      # There are no symbols in the disassembly, but we're expecting two
      # functions, one of which calls the other.
      self.assertTrue(b'call' in disasm or b'jmp' in disasm)

  def test_distributed_lto_whitelisted(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      _create_inputs(d)
      os.makedirs('obj')
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', '-m32', 'main.cpp',
          '-Foobj/main.obj'
      ])
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', '-m32', 'foo.cpp',
          '-Foobj/foo.obj'
      ])
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', '-m32', 'bar.cpp',
          '-Foobj/bar.obj'
      ])
      subprocess.check_call(
          ['llvm-ar', 'crsT', 'obj/foobar.lib', 'obj/bar.obj', 'obj/foo.obj'])
      with open('main.rsp', 'w') as f:
        f.write('obj/main.obj\n' 'obj/foobar.lib\n')
      rc = GomaLinkWindowsWhitelistMain().main([
          'goma_link.py', '--gomacc', 'gomacc', '--',
          self.lld_link(), '-nodefaultlib', '-entry:main', '-machine:X86',
          '-opt:lldlto=2', '-mllvm:-import-instr-limit=10', '-out:main.exe',
          '@main.rsp'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # Check codegen parameters.
      with open(os.path.join(d, 'lto.main.exe', 'build.ninja')) as f:
        buildrules = f.read()
        codegen_match = re.search('^rule codegen\\b.*?^[^ ]', buildrules,
                                  re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(codegen_match)
        codegen_text = codegen_match.group(0)
        self.assertIn('gomacc', codegen_text)
        self.assertIn('-m32', codegen_text)
        self.assertIn('-mllvm -import-instr-limit=10', codegen_text)
        self.assertNotIn('-flto', codegen_text)
        self.assertIn('build lto.main.exe/obj/main.obj : codegen ', buildrules)
        self.assertIn('build lto.main.exe/obj/foo.obj : codegen ', buildrules)
        link_match = re.search('^build main.exe : native-link\\b.*?^[^ ]',
                               buildrules, re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(link_match)
        link_text = link_match.group(0)
        self.assertIn('main.exe.split.obj', link_text)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main.exe'])
      # There are no symbols in the disassembly, but we're expecting a single
      # function, with no calls or jmps.
      self.assertNotIn(b'jmp', disasm)
      self.assertNotIn(b'call', disasm)


class GomaLdIntegrationTest(unittest.TestCase):
  def clangxx(self):
    return os.path.join(LLVM_BIN_DIR, 'clang++' + goma_link.exe_suffix())

  def test_nonlto(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      _create_inputs(d)
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', 'main.cpp', '-o', 'main.o'])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', 'foo.cpp', '-o', 'foo.o'])
      rc = GomaLinkUnixWhitelistMain().main([
          'goma_ld.py', '--gomacc', 'gomacc', '--',
          self.clangxx(), '-fuse-ld=lld', 'main.o', 'foo.o', '-o', 'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # lto.main directory should not be present.
      self.assertFalse(os.path.exists(os.path.join(d, 'lto.main')))
      # Check that main calls foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertIn(b'foo', main_disasm)

  def test_fallback_lto(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      _create_inputs(d)
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', 'main.cpp', '-o', 'main.o'
      ])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', '-flto=thin', 'foo.cpp', '-o', 'foo.o'])
      rc = goma_ld.GomaLinkUnix().main([
          'goma_ld.py', '--gomacc', 'gomacc', '--',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', 'main.o', 'foo.o', '-o',
          'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # lto.main directory should not be present.
      self.assertFalse(os.path.exists(os.path.join(d, 'lto.main')))
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertNotIn(b'foo', main_disasm)

  def test_distributed_lto(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      _create_inputs(d)
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', 'main.cpp', '-o', 'main.o'
      ])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', '-flto=thin', 'foo.cpp', '-o', 'foo.o'])
      rc = GomaLinkUnixWhitelistMain().main([
          'goma_ld.py', '-j', '16', '--',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', 'main.o', 'foo.o', '-o',
          'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # build.ninja file should have gomacc invocations in it.
      with open(os.path.join(d, 'lto.main', 'build.ninja')) as f:
        buildrules = f.read()
        self.assertIn('gomacc ', buildrules)
        self.assertIn('build lto.main/main.o : codegen ', buildrules)
        self.assertIn('build lto.main/foo.o : codegen ', buildrules)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertNotIn(b'foo', main_disasm)

  def test_distributed_lto_thin_archive_same_dir(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      _create_inputs(d)
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', 'main.cpp', '-o', 'main.o'
      ])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', '-flto=thin', 'foo.cpp', '-o', 'foo.o'])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', '-flto=thin', 'bar.cpp', '-o', 'bar.o'])
      subprocess.check_call(
          ['llvm-ar', 'crsT', 'libfoobar.a', 'bar.o', 'foo.o'])
      rc = GomaLinkUnixWhitelistMain().main([
          'goma_ld.py',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', 'main.o', 'libfoobar.a',
          '-o', 'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # build.ninja file should have gomacc invocations in it.
      with open(os.path.join(d, 'lto.main', 'build.ninja')) as f:
        buildrules = f.read()
        self.assertIn('gomacc ', buildrules)
        self.assertIn('build lto.main/main.o : codegen ', buildrules)
        self.assertIn('build lto.main/foo.o : codegen ', buildrules)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertNotIn(b'foo', main_disasm)

  def test_distributed_lto_thin_archive_subdir(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      _create_inputs(d)
      os.makedirs('obj')
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', 'main.cpp', '-o',
          'obj/main.o'
      ])
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', 'foo.cpp', '-o',
          'obj/foo.o'
      ])
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', 'bar.cpp', '-o',
          'obj/bar.o'
      ])
      subprocess.check_call(
          ['llvm-ar', 'crsT', 'obj/libfoobar.a', 'obj/bar.o', 'obj/foo.o'])
      rc = GomaLinkUnixWhitelistMain().main([
          'goma_ld.py',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', 'obj/main.o',
          'obj/libfoobar.a', '-o', 'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # build.ninja file should have gomacc invocations in it.
      with open(os.path.join(d, 'lto.main', 'build.ninja')) as f:
        buildrules = f.read()
        self.assertIn('gomacc ', buildrules)
        self.assertIn('build lto.main/obj/main.o : codegen ', buildrules)
        self.assertIn('build lto.main/obj/foo.o : codegen ', buildrules)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertNotIn(b'foo', main_disasm)

  def test_debug_params(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      _create_inputs(d)
      os.makedirs('obj')
      subprocess.check_call([
          self.clangxx(), '-c', '-g', '-gsplit-dwarf', '-flto=thin',
          'main.cpp', '-o', 'obj/main.o',
      ])
      subprocess.check_call([
          self.clangxx(), '-c', '-g', '-gsplit-dwarf', '-flto=thin',
          'foo.cpp', '-o', 'obj/foo.o'
      ])
      with open('main.rsp', 'w') as f:
        f.write('obj/main.o\n'
                'obj/foo.o\n')
      rc = GomaLinkUnixWhitelistMain().main([
          'goma_ld.py',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin',
          '-g', '-gsplit-dwarf', '-Wl,--lto-O2', '-o', 'main', '@main.rsp',
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # Check debug info present, refers to .dwo file, and does not
      # contain full debug info for foo.cpp.
      dbginfo = subprocess.check_output(
          ['llvm-dwarfdump', '-debug-info', 'main']
      ).decode('utf-8', 'backslashreplace')
      self.assertRegexpMatches(dbginfo, '\\bDW_AT_GNU_dwo_name\\b.*\\.dwo"')
      self.assertNotRegexpMatches(dbginfo, '\\bDW_AT_name\\b.*foo\\.cpp"')

  def test_distributed_lto_params(self):
    with NamedDirectory() as d, WorkingDirectory(d):
      _create_inputs(d)
      os.makedirs('obj')
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', '-m32', '-fsplit-lto-unit',
          '-fwhole-program-vtables', 'main.cpp', '-o', 'obj/main.o'
      ])
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', '-m32', '-fsplit-lto-unit',
          '-fwhole-program-vtables', 'foo.cpp', '-o', 'obj/foo.o'
      ])
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', '-m32', '-fsplit-lto-unit',
          '-fwhole-program-vtables', 'bar.cpp', '-o', 'obj/bar.o'
      ])
      subprocess.check_call(
          ['llvm-ar', 'crsT', 'obj/libfoobar.a', 'obj/bar.o', 'obj/foo.o'])
      with open('main.rsp', 'w') as f:
        f.write('-fsplit-lto-unit\n'
                '-fwhole-program-vtables\n'
                'obj/main.o\n'
                'obj/libfoobar.a\n')
      rc = GomaLinkUnixWhitelistMain().main([
          'goma_ld.py',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', '-m32', '-Wl,-mllvm',
          '-Wl,-generate-type-units', '-Wl,--lto-O2', '-o', 'main',
          '-Wl,--start-group', '@main.rsp', '-Wl,--end-group'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # Check codegen parameters.
      with open(os.path.join(d, 'lto.main', 'build.ninja')) as f:
        buildrules = f.read()
        codegen_match = re.search('^rule codegen\\b.*?^[^ ]', buildrules,
                                  re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(codegen_match)
        codegen_text = codegen_match.group(0)
        self.assertIn('gomacc', codegen_text)
        self.assertIn('-m32', codegen_text)
        self.assertIn('-mllvm -generate-type-units', codegen_text)
        self.assertNotIn('-flto', codegen_text)
        self.assertIn('build lto.main/obj/main.o : codegen ', buildrules)
        self.assertIn('build lto.main/obj/foo.o : codegen ', buildrules)
        link_match = re.search('^build main : native-link\\b.*?^[^ ]',
                               buildrules, re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(link_match)
        link_text = link_match.group(0)
        self.assertIn('main.split.o', link_text)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertNotIn(b'foo', main_disasm)


if __name__ == '__main__':
  unittest.main()
