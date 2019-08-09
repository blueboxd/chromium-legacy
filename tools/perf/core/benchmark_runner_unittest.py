# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mock

from core import benchmark_runner


def _FakeParseArgs(environment, args, results_arg_parser):
  del environment  # Unused.
  options, _ = results_arg_parser.parse_known_args(args)
  return options


class BenchmarkRunnerUnittest(unittest.TestCase):
  def testMain_returnCode(self):
    """Test that benchmark_runner.main() respects return code from Telemetry."""
    # TODO(crbug.com/985712): Ideally we should write a more "integration" kind
    # of test, where we don't mock out the Telemetry command line. This is
    # hard to do now, however, because we need a way to convert the browser
    # selection done by the test runner, back to a suitable --browser arg for
    # the command line below. Namely, we need an equivalent of
    # options_for_unittests.GetCopy() that returns a list of string args
    # rather than the parsed options object.
    config = mock.Mock()
    with mock.patch('core.benchmark_runner.command_line') as telemetry_cli:
      telemetry_cli.ParseArgs.side_effect = _FakeParseArgs
      telemetry_cli.RunCommand.return_value = 42

      # Note: For now we pass `--output-format none` and a non-existent output
      # dir to prevent the results processor from processing any results.
      return_code = benchmark_runner.main(config, [
          'run', 'some.benchmark', '--browser', 'stable',
          '--output-dir', '/does/not/exist', '--output-format', 'none'])
      self.assertEqual(return_code, 42)
