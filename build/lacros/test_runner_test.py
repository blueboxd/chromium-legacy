#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import unittest

import mock
from parameterized import parameterized

import test_runner


class TestRunnerTest(unittest.TestCase):
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  # Tests 'download_for_bots' to download ash-chrome for bots to isolate.
  # TODO(crbug.com/1107010): remove this test once ash-chrome version is pinned
  # to chromium/src.
  def test_download_for_bots(self, mock_download):
    args = ['script_name', 'download_for_bots']
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      mock_download.assert_called_with('for_bots')

  @parameterized.expand([
      'url_unittests',
      './url_unittests',
      'out/release/url_unittests',
      './out/release/url_unittests',
  ])
  @mock.patch.object(os.path, 'isfile', return_value=True)
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  @mock.patch.object(subprocess, 'call')
  # Tests that the test runner doesn't attempt to download ash-chrome if not
  # required.
  def test_do_not_require_ash_chrome(self, command, mock_subprocess,
                                     mock_download, _):
    args = ['script_name', 'test', command]
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      mock_subprocess.assert_called_with([command])
      self.assertFalse(mock_download.called)

  @mock.patch.object(os.path, 'isfile', return_value=True)
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  @mock.patch.object(subprocess, 'call')
  # Tests that arguments not known to the test runner are forwarded to the
  # command that invokes tests.
  def test_command_arguments(self, mock_subprocess, mock_download, _):
    args = [
        'script_name', 'test', './url_unittests', '--gtest_filter=Suite.Test'
    ]
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      mock_download.called
      mock_subprocess.assert_called_with(
          ['./url_unittests', '--gtest_filter=Suite.Test'])
      self.assertFalse(mock_download.called)

  @mock.patch.object(os.path, 'isfile', return_value=True)
  # Tests that if a ash-chrome version is specified, uses ash-chrome to run
  # tests anyway even if |_TARGETS_REQUIRE_ASH_CHROME| indicates an ash-chrome
  # is not required.
  def test_overrides_do_not_require_ash_chrome(self, _):
    args = [
        'script_name', 'test', './url_unittests', '--ash-chrome-version',
        '86.0.4187.0'
    ]
    with mock.patch.object(sys, 'argv', args):
      with self.assertRaises(RuntimeError) as context:
        test_runner.Main()
        self.assertEqual('Run tests with ash-chrome is to be implemented',
                         str(context.exception))


if __name__ == '__main__':
  unittest.main()
