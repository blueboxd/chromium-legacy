#!/usr/bin/env vpython
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script facilitates running tests for lacros.

WARNING: currently, this script only supports running test targets that do not
require a display server, such as base_unittests and url_unittests.
TODO(crbug.com/1104318): Support test targets that require a display server by
using ash_chrome.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir))
sys.path.append(os.path.join(_SRC_ROOT, 'third_party', 'depot_tools'))
import download_from_google_storage

# Base GS URL to store nightly uploaded official Chrome.
# TODO(crbug.com/1035562): This URL is created for testing purpose only.
# Replace this and the following GS paths with real URL and paths once nightly
# official builds of ash-chrome are uploaded continuously.
_GS_URL_BASE = 'gs://lacros-testing/desktop-5c0tCh'

# GS path to the file containing the latest version of ash-chrome.
_GS_ASH_CHROME_LATEST_VERSION_FILE = _GS_URL_BASE + '/latest/linux-chromeos.txt'

# GS path to the zipped ash-chrome build with any given version.
_GS_ASH_CHROME_PATH = 'linux-chromeos/chrome-linux-chromeos.zip'

# Directory to cache downloaded ash-chrome versions to avoid re-downloading.
# TODO(crbug.com/1104318): Cleans up unused versions regularly to avoid
# consuming too much disk space.
_PREBUILT_ASH_CHROME_DIR = os.path.join(os.path.dirname(__file__),
                                        'prebuilt_ash_chrome')


def _GetAshChromeDirPath(version):
  """Returns a path to the dir storing the downloaded version of ash-chrome."""
  return os.path.join(_PREBUILT_ASH_CHROME_DIR, version)


def _DownloadAshChromeIfNecessary(version):
  """Download a given version of ash-chrome if not already exists.

  Args:
    version: A string representing the version, such as "86.0.4187.0".

  Raises:
      RuntimeError: If failed to download the specified version, for example,
          if the version is not present on gcs.
  """

  def IsAshChromeDirValid(ash_chrome_dir):
    # This function assumes that once 'chrome' is present, other dependencies
    # will be present as well, it's not always true, for example, if the test
    # runner process gets killed in the middle of unzipping (~2 seconds), but
    # it's unlikely for the assumption to break in practice.
    return os.path.isdir(ash_chrome_dir) and os.path.isfile(
        os.path.join(ash_chrome_dir, 'chrome'))

  ash_chrome_dir = _GetAshChromeDirPath(version)
  if IsAshChromeDirValid(ash_chrome_dir):
    return

  shutil.rmtree(ash_chrome_dir, ignore_errors=True)
  os.makedirs(ash_chrome_dir)
  with tempfile.NamedTemporaryFile() as tmp:
    gsutil = download_from_google_storage.Gsutil(
        download_from_google_storage.GSUTIL_DEFAULT_PATH)
    gs_path = _GS_URL_BASE + '/' + version + '/' + _GS_ASH_CHROME_PATH
    exit_code = gsutil.call('cp', gs_path, tmp.name)
    if exit_code:
      raise RuntimeError('Failed to download: "%s"' % gs_path)

    # https://bugs.python.org/issue15795. ZipFile doesn't preserve permissions.
    # And in order to workaround the issue, this function is created and used
    # instead of ZipFile.extractall().
    # The solution is copied from:
    # https://stackoverflow.com/questions/42326428/zipfile-in-python-file-permission
    def ExtractFile(zf, info, extract_dir):
      zf.extract(info.filename, path=extract_dir)
      perm = info.external_attr >> 16
      os.chmod(os.path.join(extract_dir, info.filename), perm)

    with zipfile.ZipFile(tmp.name, 'r') as zf:
      # Extra all files instead of just 'chrome' binary because 'chrome' needs
      # other resources and libraries to run.
      for info in zf.infolist():
        ExtractFile(zf, info, ash_chrome_dir)


def _GetLatestVersionOfAshChrome():
  """Returns the latest version of uploaded official ash-chrome."""
  with tempfile.NamedTemporaryFile() as tmp:
    gsutil = download_from_google_storage.Gsutil(
        download_from_google_storage.GSUTIL_DEFAULT_PATH)
    gsutil.check_call('cp', _GS_ASH_CHROME_LATEST_VERSION_FILE, tmp.name)
    with open(tmp.name, 'r') as f:
      return f.read().strip()


def _ParseArguments():
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  arg_parser.add_argument(
      'command',
      help='A single command to invoke the tests, for example: '
      '"./url_unittests". Any argument unknown to this test runner script will '
      'be forwarded to the command, for example: "--gtest_filter=Suite.Test"')

  args = arg_parser.parse_known_args()
  return args[0], args[1]


def Main():
  args, forward_args = _ParseArguments()
  return subprocess.call([args.command] + forward_args)


if __name__ == '__main__':
  sys.exit(Main())
