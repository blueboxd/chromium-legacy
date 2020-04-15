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
import os
import re
import subprocess
import sys


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
  args = parser.parse_args()

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

  cmd = [executable_path] + executable_args
  cmd = [sys.executable] + cmd
  logging.info(' '.join(cmd))
  return subprocess.call(cmd)


if __name__ == '__main__':
  sys.exit(main())
