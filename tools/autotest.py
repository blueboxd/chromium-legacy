#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds and runs a test by filename.

This script finds the appropriate test suite for the specified test file, builds
it, then runs it with the (optionally) specified filter, passing any extra args
on to the test runner.

Examples:
autotest.py -C out/Desktop bit_cast_unittest.cc --gtest_filter=BitCastTest* -v
autotest.py -C out/Android UrlUtilitiesUnitTest --fast-local-dev -v
"""

import argparse
import locale
import logging
import multiprocessing
import os
import re
import shlex
import subprocess
import sys

from pathlib import Path

USE_PYTHON_3 = f'This script will only run under python3.'

SRC_DIR = Path(__file__).parent.parent.resolve()
DEPOT_TOOLS_DIR = SRC_DIR.joinpath('third_party', 'depot_tools')
DEBUG = False

_TEST_TARGET_SUFFIXES = [
    '_browsertests',
    '_junit_tests',
    '_perftests',
    '_test_apk',
    '_unittests',
]

# Some test suites use suffixes that would also match non-test-suite targets.
# Those test suites should be manually added here.
_OTHER_TEST_TARGETS = [
    '//chrome/test:browser_tests',
    '//chrome/test:unit_tests',
]


class CommandError(Exception):
  """Exception thrown when we can't parse the input file."""

  def __init__(self, command, return_code, output=None):
    Exception.__init__(self)
    self.command = command
    self.return_code = return_code
    self.output = output

  def __str__(self):
    message = (f'\n***\nERROR: Error while running command {self.command}'
               f'.\nExit status: {self.return_code}\n')
    if self.output:
      message += f'Output:\n{self.output}\n'
    message += '***'
    return message


def LogCommand(cmd, **kwargs):
  if DEBUG:
    print('Running command: ' + ' '.join(cmd))

  try:
    subprocess.check_call(cmd, **kwargs)
  except subprocess.CalledProcessError as e:
    raise CommandError(e.cmd, e.returncode) from None


def RunCommand(cmd, **kwargs):
  if DEBUG:
    print('Running command: ' + ' '.join(cmd))

  try:
    # Set an encoding to convert the binary output to a string.
    return subprocess.check_output(
        cmd, **kwargs, encoding=locale.getpreferredencoding())
  except subprocess.CalledProcessError as e:
    raise CommandError(e.cmd, e.returncode, e.output) from None


def BuildTestTargetWithNinja(out_dir, target, dry_run):
  """Builds the specified target with ninja"""
  ninja_path = os.path.join(DEPOT_TOOLS_DIR, 'autoninja')
  if 'win' in sys.platform:
    ninja_path += '.bat'
  cmd = [ninja_path, '-C', out_dir, target]
  print('Building: ' + ' '.join(cmd))
  if (dry_run):
    return
  RunCommand(cmd)


def RecursiveMatchFilename(folder, filename):
  current_dir = os.path.split(folder)[-1]
  if current_dir.startswith('out') or current_dir.startswith('.'):
    return []
  matches = []
  with os.scandir(folder) as it:
    for entry in it:
      if (entry.is_symlink()):
        continue
      if entry.is_file() and filename in entry.path:
        matches.append(entry.path)
      if entry.is_dir():
        # On Windows, junctions are like a symlink that python interprets as a
        # directory, leading to exceptions being thrown. We can just catch and
        # ignore these exceptions like we would ignore symlinks.
        try:
          matches += RecursiveMatchFilename(entry.path, filename)
        except FileNotFoundError as e:
          if DEBUG:
            print(f'Failed to scan directory "{entry}" - junction?')
          pass
  return matches


def FindMatchingTestFile(target):
  if DEBUG:
    print('Finding files with full path containing: ' + target)
  results = RecursiveMatchFilename(SRC_DIR, target)
  if DEBUG:
    print('Found matching file(s): ' + ' '.join(results))
  if len(results) > 1:
    # Arbitrarily capping at 10 results so we don't print the name of every file
    # in the repo if the target is poorly specified.
    results = results[:10]
    raise Exception(f'Target "{target}" is ambiguous. Matching files: '
                    f'{results}')
  if not results:
    raise Exception(f'Target "{target}" did not match any files.')
  return results[0]


def IsTestTarget(target):
  for suffix in _TEST_TARGET_SUFFIXES:
    if target.endswith(suffix):
      return True
  return target in _OTHER_TEST_TARGETS


def HaveUserPickTarget(path, targets):
  # Cap to 10 targets for convenience [0-9].
  targets = targets[:10]
  target_list = ''
  i = 0
  for target in targets:
    target_list += f'{i}. {target}\n'
    i += 1
  try:
    value = int(
        input(f'Target "{path}" is used by multiple test targets.\n' +
              target_list + 'Please pick a target: '))
    return targets[value]
  except Exception as e:
    print('Try again')
    return HaveUserPickTarget(path, targets)


def FindTestTarget(out_dir, path):
  # Use gn refs to recursively find all targets that depend on |path|, filter
  # internal gn targets, and match against well-known test suffixes, falling
  # back to a list of known test targets if that fails.
  gn_path = os.path.join(DEPOT_TOOLS_DIR, 'gn')
  if 'win' in sys.platform:
    gn_path += '.bat'
  cmd = [gn_path, 'refs', out_dir, '--all', path]
  targets = RunCommand(cmd, cwd=SRC_DIR).splitlines()
  targets = [t for t in targets if '__' not in t]
  test_targets = [t for t in targets if IsTestTarget(t)]

  if not test_targets:
    raise Exception(
        f'Target "{path}" did not match any test targets. Consider adding '
        f'one of the following targets to the top of this file: {targets}')
  target = test_targets[0]
  if len(test_targets) > 1:
    target = HaveUserPickTarget(path, test_targets)

  return target.split(':')[-1]


def RunTestTarget(out_dir, target, gtest_filter, extra_args, dry_run):
  # Look for the Android wrapper script first.
  path = os.path.join(out_dir, 'bin', f'run_{target}')
  if not os.path.isfile(path):
    # Otherwise, use the Desktop target which is an executable.
    path = os.path.join(out_dir, target)
  extra_args = ' '.join(extra_args)
  cmd = [path, f'--gtest_filter={gtest_filter}'] + shlex.split(extra_args)
  print('Running test: ' + ' '.join(cmd))
  if (dry_run):
    return
  LogCommand(cmd)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument(
      '--out-dir',
      '-C',
      metavar='OUT_DIR',
      help='output directory of the build',
      required=True)
  parser.add_argument(
      '--gtest_filter', '-f', metavar='FILTER', help='test filter')
  parser.add_argument(
      '--dry_run',
      '-n',
      action='store_true',
      help='Print ninja and test run commands without executing them.')
  parser.add_argument(
      'file', metavar='FILE_NAME', help='test suite file (eg. FooTest.java)')

  args, _extras = parser.parse_known_args()

  if not os.path.isdir(args.out_dir):
    parser.error(f'OUT_DIR "{args.out_dir}" does not exist.')
  filename = FindMatchingTestFile(args.file)

  gtest_filter = args.gtest_filter
  if not gtest_filter:
    if not filename.endswith('java'):
      # In c++ tests, the test class often doesn't match the filename, or a
      # single file will contain multiple test classes. It's likely possible to
      # handle most cases with a regex and provide a default here.
      # Patches welcome :)
      parser.error('--gtest_filter must be specified for non-java tests.')
    gtest_filter = '*' + os.path.splitext(os.path.basename(filename))[0] + '*'

  target = FindTestTarget(args.out_dir, filename)
  BuildTestTargetWithNinja(args.out_dir, target, args.dry_run)
  RunTestTarget(args.out_dir, target, gtest_filter, _extras, args.dry_run)


if __name__ == '__main__':
  sys.exit(main())
