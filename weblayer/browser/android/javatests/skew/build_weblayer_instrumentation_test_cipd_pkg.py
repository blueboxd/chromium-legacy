#!/usr/bin/env python3
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Script to build a CIPD package for weblayer_instrumentation_test_apk from
# the current Chromium checkout.
#
# This should be run from the src directory of a release branch. After the
# package is built the user should run two cipd commands (printed at the end
# of script execution) to upload the package to the CIPD server and to update
# the ref for the corresponding milestone. Once the ref is updated, the version
# skew test will pick up the new package in successive runs.

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile


# Run mb.py out of the current branch for simplicity.
MB_PATH = './tools/mb/mb.py'

# Get the config specifying the gn args from the location of this script.
MB_CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              'mb_config.pyl')

# CIPD package path.
# https://chrome-infra-packages.appspot.com/p/chromium/testing/weblayer-x86/+/
CIPD_PKG_PATH='chromium/testing/weblayer-x86'

def zip_test_target(zip_filename):
  """Create zip of all deps for weblayer_instrumentation_test_apk.

  Args:
    zip_filename: destination zip filename.
  """
  cmd = [MB_PATH,
         'zip',
         '--master=dummy.master',
         '--builder=dummy.builder',
         '--config-file=%s' % MB_CONFIG_PATH,
         'out/Release',
         'weblayer_instrumentation_test_apk',
         zip_filename]
  print(' '.join(cmd))
  subprocess.check_call(cmd)


def build_cipd_pkg(input_path, cipd_filename):
  """Create a CIPD package file from the given input path.

  Args:
    input_path: input directory from which to build the package.
    cipd_filename: output filename for resulting cipd archive.
  """
  cmd = ['cipd',
         'pkg-build',
         '--in=%s' % input_path,
         '--install-mode=copy',
         '--name=%s' % CIPD_PKG_PATH,
         '--out=%s' % cipd_filename]
  print(' '.join(cmd))
  subprocess.check_call(cmd)


def main():
  parser = argparse.ArgumentParser(
      description='Package weblayer instrumentation tests for CIPD.')
  parser.add_argument(
      '--cipd_out',
      required=True,
      help="Output filename for resulting .cipd file.")
  args = parser.parse_args()

  with tempfile.TemporaryDirectory() as tmp_dir:
    # Create zip archive of test target.
    zip_filename = os.path.join(tmp_dir, 'file.zip')
    zip_test_target(zip_filename)

    # Extract zip archive.
    extracted = os.path.join(tmp_dir, 'extracted')
    os.mkdir(extracted)
    with zipfile.ZipFile(zip_filename) as zip_file:
      zip_file.extractall(path=extracted)

    # Create CIPD archive.
    tmp_cipd_filename = os.path.join(tmp_dir, 'file.cipd')
    build_cipd_pkg(extracted, tmp_cipd_filename)
    shutil.move(tmp_cipd_filename, args.cipd_out)

    print(('Use "cipd pkg-register %s -verbose -tag \'version:<branch>\'" ' +
           'to upload package to the cipd server.') % args.cipd_out)
    print('Use "cipd set-ref chromium/testing/weblayer-x86 --version ' +
          '<CIPD instance version> -ref m<milestone>" to update the ref.')
    print('The CIPD instance version can be found on the "Instance" line ' +
          'above after "chromium/testing/weblayer-x86:".')


if __name__ == '__main__':
  sys.exit(main())
