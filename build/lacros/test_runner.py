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
import logging
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir))
sys.path.append(os.path.join(_SRC_ROOT, 'third_party', 'depot_tools'))

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
_PREBUILT_ASH_CHROME_DIR = os.path.join(os.path.dirname(__file__),
                                        'prebuilt_ash_chrome')

# TODO(crbug.com/1104291): Complete this list once the lacros FYI builder is
# running all the test targets.
# List of targets that require ash-chrome as a Wayland server in order to run.
_TARGETS_REQUIRE_ASH_CHROME = [
    'browser_tests',
    'components_unittests',
    'sync_integration_tests',
    'unit_tests',
]


def _GetAshChromeDirPath(version):
  """Returns a path to the dir storing the downloaded version of ash-chrome."""
  return os.path.join(_PREBUILT_ASH_CHROME_DIR, version)


def _remove_unused_ash_chrome_versions(version_to_skip):
  """Removes unused ash-chrome versions to save disk space.

  Currently, when an ash-chrome zip is downloaded and unpacked, the atime/mtime
  of the dir and the files are NOW instead of the time when they were built, but
  there is no garanteen it will always be the behavior in the future, so avoid
  removing the current version just in case.

  Args:
    version_to_skip (str): the version to skip removing regardless of its age.
  """
  days = 7
  expiration_duration = 60 * 60 * 24 * days

  for f in os.listdir(_PREBUILT_ASH_CHROME_DIR):
    if f == version_to_skip:
      continue

    p = os.path.join(_PREBUILT_ASH_CHROME_DIR, f)
    if os.path.isfile(p):
      # The prebuilt ash-chrome dir is NOT supposed to contain any files, remove
      # them to keep the directory clean.
      os.remove(p)
      continue

    age = time.time() - os.path.getatime(os.path.join(p, 'chrome'))
    if age > expiration_duration:
      logging.info(
          'Removing ash-chrome: "%s" as it hasn\'t been used in the '
          'past %d days', p, days)
      shutil.rmtree(p)


def _DownloadAshChromeIfNecessary(version):
  """Download a given version of ash-chrome if not already exists.

  Currently, a special constant version value is support: "for_bots", the reason
  is that version number is still not pinned to chromium/src, so a constant
  value is needed to make sure that after the builder who downloads and isolates
  ash-chrome, the tester knows where to look for the binary to use.
  TODO(crbug.com/1107010): remove the support once ash-chrome version is pinned
  to chromium/src.

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
  if version != 'for_bots' and IsAshChromeDirValid(ash_chrome_dir):
    return

  shutil.rmtree(ash_chrome_dir, ignore_errors=True)
  os.makedirs(ash_chrome_dir)
  with tempfile.NamedTemporaryFile() as tmp:
    import download_from_google_storage
    gsutil = download_from_google_storage.Gsutil(
        download_from_google_storage.GSUTIL_DEFAULT_PATH)
    gs_version = (_GetLatestVersionOfAshChrome()
                  if version == 'for_bots' else version)
    logging.info('Ash-chrome version: %s', gs_version)
    gs_path = _GS_URL_BASE + '/' + gs_version + '/' + _GS_ASH_CHROME_PATH
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

  _remove_unused_ash_chrome_versions(version)


def _GetLatestVersionOfAshChrome():
  """Returns the latest version of uploaded official ash-chrome."""
  with tempfile.NamedTemporaryFile() as tmp:
    import download_from_google_storage
    gsutil = download_from_google_storage.Gsutil(
        download_from_google_storage.GSUTIL_DEFAULT_PATH)
    gsutil.check_call('cp', _GS_ASH_CHROME_LATEST_VERSION_FILE, tmp.name)
    with open(tmp.name, 'r') as f:
      return f.read().strip()


def _RunTest(args, forward_args):
  """Run tests with given args.

  args (dict): Args for this script.
  forward_args (dict): Args to be forwarded to the test command.

  Raises:
      RuntimeError: If the given test binary doesn't exist or the test runner
          doesn't know how to run it.
  """

  if not os.path.isfile(args.command):
    raise RuntimeError('Specified test command: "%s" doesn\'t exist' %
                       args.command)

  # |_TARGETS_REQUIRE_ASH_CHROME| may not always be accurate as it is updated
  # with a best effort only, therefore, allow the invoker to override the
  # behavior with a specified ash-chrome version, which makes sure that
  # automated CI/CQ builders would always work correctly.
  if (os.path.basename(args.command) not in _TARGETS_REQUIRE_ASH_CHROME
      and not args.ash_chrome_version):
    return subprocess.call([args.command] + forward_args)

  raise RuntimeError('Run tests with ash-chrome is to be implemented')


def Main():
  logging.basicConfig(level=logging.INFO)
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  subparsers = arg_parser.add_subparsers()

  download_parser = subparsers.add_parser(
      'download_for_bots',
      help='Download prebuilt ash-chrome for bots so that tests are hermetic '
      'during execution')
  download_parser.set_defaults(
      func=lambda *_: _DownloadAshChromeIfNecessary('for_bots'))

  test_parser = subparsers.add_parser('test', help='Run tests')
  test_parser.set_defaults(func=_RunTest)

  test_parser.add_argument(
      'command',
      help='A single command to invoke the tests, for example: '
      '"./url_unittests". Any argument unknown to this test runner script will '
      'be forwarded to the command, for example: "--gtest_filter=Suite.Test"')

  test_parser.add_argument(
      '-a',
      '--ash-chrome-version',
      type=str,
      help='Version of ash_chrome to use for testing, for example: '
      '"86.0.4187.0". If not specified, will use the latest version available')

  args = arg_parser.parse_known_args()
  return args[0].func(args[0], args[1])


if __name__ == '__main__':
  sys.exit(Main())
