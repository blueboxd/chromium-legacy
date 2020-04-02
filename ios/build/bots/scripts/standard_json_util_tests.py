# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for standard_json_util.py."""

import collections
import standard_json_util as sju


class UnitTest(unittest.TestCase):

  def test_base_cases(self):
    """Test invalid test names are skipped"""
    passed = ['', None]
    failed = ['', None]

    output = sju.StdJson(passed=passed, failed=failed)
    self.assertFalse(output.tests)

  def test_single_constructor(self):
    """Test one test passing, failing, flaking via constructor"""
    test = 'a'

    output = sju.StdJson(passed=test)
    self.assertEqual(output.tests['a']['actual'], 'PASS')

    output = sju.StdJson(failed=test)
    self.assertEqual(output.tests['a']['actual'], 'FAIL')

    output = sju.StdJson(passed=test, flaky=True)
    self.assertEqual(output.tests['a']['actual'], 'PASS')
    self.assertTrue(output.tests['a']['is_flaky'])

  def test_multi_run(self):
    """Test multiple executions of the same test"""
    test = 'a'

    output = sju.StdJson(passed=test)
    self.assertEqual(output.tests['a']['actual'], 'PASS')

    output.mark_failed(test)
    self.assertEqual(output.tests['a']['actual'], 'PASS FAIL')

    output.mark_pass(test, flaky=True)
    self.assertEqual(output.tests['a']['actual'], 'PASS FAIL PASS')
    self.assertTrue(output.tests['a']['is_flaky'])

  def test_multi_scenario(self):
    """Test a scenario where some tests pass, fail and flake"""
    passed = ['a', 'b', 'c']
    failed = ['d']
    flaked = ['e']

    output = sju.StdJson(passed=passed, failed=failed, flaked=flaked)
    self.assertEqual(len(output.tests), 5)
    # Ensure that the flaked is set as passed, with is_flaky=True
    self.assertTrue(output.tests['e']['is_flaky'])

    # A retry that re-runs failed fails again
    output.mark_failed('d')
    self.assertEqual(output.tests['d']['actual'], 'FAIL FAIL')

    # Another retry of 'd' passes, so we set it as a flaky pass
    output.mark_passed('d', flaky=True)
    self.assertEqual(output.tests['d']['actual'], 'FAIL FAIL PASS')
    self.assertTrue(output.tests['d']['is_flaky'])

  def test_flaky_without_explicit(self):
    """Test setting pass on an already failed test, w/o explicit flaky"""
    test = 'e'
    output = sju.StdJson()
    output.mark_failed(test)
    self.assertEqual(output.tests['e']['actual'], 'FAIL')

    output.mark_passed(test)
    self.assertEqual(output.tests['e']['actual'], 'FAIL PASS')
    self.assertTrue(output.tests['e']['is_flaky'])

  def test_timeout(self):
    """Test setting timeout"""
    test = 'e'
    output = sju.StdJson()
    output.mark_timeout(test)
    self.assertEqual(output.tests['e']['actual'], 'TIMEOUT')

    output = sju.StdJson()
    output.mark_failed(test)
    self.assertEqual(output.tests['e']['actual'], 'FAIL')
    output.mark_timeout(test)
    self.assertTrue(output.tests['e']['actual'], 'FAIL TIMEOUT')


if __name__ == '__main__':
  unittest.main()
