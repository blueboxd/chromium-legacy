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
  @parameterized.expand([
      'url_unittests',
      './url_unittests',
      'out/release/url_unittests',
      './out/release/url_unittests',
  ])
  @mock.patch.object(test_runner, '_DownloadAshChromeIfNecessary')
  @mock.patch.object(subprocess, 'call')
  # Tests that the test runner doesn't attempt to download ash-chrome if not
  # required.
  def test_do_not_require_ash_chrome(self, command, mock_subprocess,
                                     mock_download):
    args = ['script_name', command]
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      mock_subprocess.assert_called_with([command])
      self.assertFalse(mock_download.called)

  @mock.patch.object(subprocess, 'call')
  # Tests that arguments not known to the test runner are forwarded to the
  # command that invokes tests.
  def test_command_arguments(self, mock_subprocess):
    args = ['script_name', './url_unittests', '--gtest_filter=Suite.Test']
    with mock.patch.object(sys, 'argv', args):
      test_runner.Main()
      mock_subprocess.assert_called_with(
          ['./url_unittests', '--gtest_filter=Suite.Test'])


if __name__ == '__main__':
  unittest.main()
