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
import subprocess
import sys


def _ParseArguments():
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  arg_parser.add_argument(
      'command',
      help='Command to execute the tests. For example: "./url_unittests')

  args = arg_parser.parse_known_args()
  return args[0], args[1]


def Main():
  args, forward_args = _ParseArguments()
  return subprocess.call([args.command] + forward_args)


if __name__ == '__main__':
  sys.exit(Main())
