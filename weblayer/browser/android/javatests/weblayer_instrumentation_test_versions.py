#!/usr/bin/env vpython
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Runs WebLayer instrumentation tests against arbitrary versions of tests, the
# client, and the implementation.
#
# Example usage, testing M80 tests and client against master implementation:
#   autoninja -C out/Release weblayer_instrumentation_test_versions_apk
#   cipd install --root /tmp/M80 chromium/testing/weblayer-x86 m80
#   out/Release/bin/run_weblayer_instrumentation_tests_versions_apk \
#       --tests-outdir /tmp/M80/out/Release
#       --client-outdir /tmp/M80/out/Release
#       --implementation-outdir out/Release

import argparse
import logging
import operator
import os
import re
import subprocess
import sys

CUR_DIR = os.path.dirname(os.path.realpath(__file__))

# Find src root starting from either the release bin directory or original path.
if os.path.basename(CUR_DIR) == 'bin':
  SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(CUR_DIR)))
else:
  SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
      CUR_DIR))))

TYP_DIR = os.path.join(
    SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')

if TYP_DIR not in sys.path:
  sys.path.insert(0, TYP_DIR)

import typ


# Mapping of operator string in the expectation file tags to actual operator.
OP_MAP = {'gte': operator.ge, 'lte': operator.le}


def tag_matches(tag, impl_version='trunk', client_version='trunk'):
  """Test if specified versions match the tag.

  Args:
    tag: skew test expectation tag, e.g. 'impl_lte_5' or 'client_lte_2'.
    impl_version: WebLayer implementation version number or 'trunk'.
    client_version: WebLayer implementation version number or 'trunk'.

  Returns:
    True if the specified versions match the tag.

  Raises:
    AssertionError if the tag is invalid.
  """
  # Extract the three components from the tag.
  match = re.match(r'(client|impl)_([gl]te)_([0-9]+)', tag)
  assert match is not None, (
      'tag must be of the form "{client,impl}_{gte,lte}_$version", found %r' %
      tag)
  target_str, op_str, tag_version_str = match.groups()

  # If a version is specified see if the tag refers to the same target or
  # return False otherwise.
  if impl_version != 'trunk' and target_str != 'impl':
    return False
  if client_version != 'trunk' and target_str != 'client':
    return False


  version = impl_version if impl_version != 'trunk' else client_version
  assert type(version) == int, 'Specified version must be an integer.'

  tag_version = int(tag_version_str)
  op = OP_MAP[op_str]
  return op(version, tag_version)


def tests_to_skip(expectation_contents, impl_version='trunk',
                  client_version='trunk'):
  """Get list of tests to skip for the given version.

  Args:
    expectation_contents: String containing expectation file contents.
    impl_version: WebLayer implementation version number or 'trunk'.
    client_version: WebLayer implementation version number or 'trunk'.

  Returns:
    List of test names to skip.

  Raises:
    AssertionError if both versions are 'trunk'.
  """
  assert impl_version != 'trunk' or client_version != 'trunk'

  parser = typ.expectations_parser.TaggedTestListParser(expectation_contents)

  tests = []
  for expectation in parser.expectations:
    assert len(expectation.tags) == 1, (
        'Only one tag is allowed per expectation.')
    assert len(expectation.results) == 1 and (
        typ.json_results.ResultType.Skip in expectation.results), (
            'Only "Skip" is supported in the skew test expectations.')

    # Iterate over the first (and only) item since can't index over a frozenset.
    tag = iter(expectation.tags).next()
    if tag_matches(tag, impl_version, client_version):
      tests.append(expectation.test)
  return tests


def main():
  """Wrapper to call weblayer instrumentation tests with different versions."""

  parser = argparse.ArgumentParser(
      description='Run weblayer instrumentation tests at different versions.')
  parser.add_argument(
      '--tests-outdir',
      required=True,
      help=('Build output directory from which to find tests and test ' +
            'support files. Since test dependencies can reside in both ' +
            'the build output directory and the source directory, this ' +
            'script will look two directories up for source files unless ' +
            '--test-srcdir is also set.'))
  parser.add_argument(
      '--tests-srcdir',
      required=False,
      default='',
      help=('Source directory from which to find test data. If unset the ' +
            'script will use two directories above --test-outdir.'))
  parser.add_argument(
      '--client-outdir',
      required=True,
      help='Build output directory for WebLayer client.')
  parser.add_argument(
      '--implementation-outdir',
      required=True,
      help='Build output directory for WebLayer implementation.')
  parser.add_argument(
      '--test-expectations',
      required=False,
      default='',
      help=('Test expectations file describing which tests are failing at '
            'different versions.'))

  version_group = parser.add_mutually_exclusive_group(required=True)
  version_group.add_argument(
      '--client-version',
      default='trunk',
      help=('Version of the client being used if not trunk. Only set one of '
            '--client-version and --impl-version.'))
  version_group.add_argument(
      '--impl-version',
      default='trunk',
      help=('Version of the implementation  being used if not trunk. Only set '
            'one of --client-version and --impl-version.'))
  args, remaining_args = parser.parse_known_args()

  logging.basicConfig(level=logging.INFO)

  # The command line is derived from the resulting command line from
  # run_weblayer_instrumentation_test_apk but with parameterized tests, client,
  # and implementation.
  if args.tests_srcdir:
    tests_srcdir = args.tests_srcdir
  else:
    tests_srcdir = os.path.normpath(os.path.join(args.tests_outdir, '..', '..'))
  executable_path = os.path.join(tests_srcdir, 'build/android/test_runner.py')
  executable_args = [
      'instrumentation',
      '--output-directory',
      args.tests_outdir,
      '--runtime-deps-path',
      os.path.join(args.tests_outdir,
                   ('gen.runtime/weblayer/browser/android/javatests/' +
                    'weblayer_instrumentation_test_apk.runtime_deps')),
      '--test-apk',
      os.path.join(args.tests_outdir, 'apks/WebLayerInstrumentationTest.apk'),
      '--test-jar',
      os.path.join(args.tests_outdir,
                   'test.lib.java/WebLayerInstrumentationTest.jar'),
      '--apk-under-test',
      os.path.join(args.client_outdir, 'apks/WebLayerShellSystemWebView.apk'),
      '--use-webview-provider',
      os.path.join(args.implementation_outdir, 'apks/SystemWebView.apk'),
      '--additional-apk',
      os.path.join(args.tests_outdir, 'apks/ChromiumNetTestSupport.apk')]

  cmd = [executable_path] + executable_args + remaining_args
  cmd = [sys.executable] + cmd

  tests = []
  if args.test_expectations:
    if args.impl_version != 'trunk':
      args.impl_version = int(args.impl_version)
    if args.client_version != 'trunk':
      args.client_version = int(args.client_version)
    with open(args.test_expectations) as expectations_file:
      contents = expectations_file.read()
      tests = tests_to_skip(contents, impl_version=args.impl_version,
                            client_version=args.client_version)
  if tests:
    logging.info('Filtering known failing tests: %s', tests)
    cmd.append('--test-filter=-%s' % ':'.join(tests))

  logging.info(' '.join(cmd))
  return subprocess.call(cmd)


if __name__ == '__main__':
  sys.exit(main())
