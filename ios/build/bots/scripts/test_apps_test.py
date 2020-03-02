# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for test_apps.py."""

import unittest

import test_apps
import test_runner_test


class GetKIFTestFilterTest(test_runner_test.TestCase):
  """Tests for test_runner.get_kif_test_filter."""

  def test_correct(self):
    """Ensures correctness of filter."""
    tests = [
      'KIF.test1',
      'KIF.test2',
    ]
    expected = 'NAME:test1|test2'

    self.assertEqual(expected, test_apps.get_kif_test_filter(tests))

  def test_correct_inverted(self):
    """Ensures correctness of inverted filter."""
    tests = [
      'KIF.test1',
      'KIF.test2',
    ]
    expected = '-NAME:test1|test2'

    self.assertEqual(expected,
                    test_apps.get_kif_test_filter(tests, invert=True))


class GetGTestFilterTest(test_runner_test.TestCase):
  """Tests for test_runner.get_gtest_filter."""

  def test_correct(self):
    """Ensures correctness of filter."""
    tests = [
      'test.1',
      'test.2',
    ]
    expected = 'test.1:test.2'

    self.assertEqual(test_apps.get_gtest_filter(tests), expected)

  def test_correct_inverted(self):
    """Ensures correctness of inverted filter."""
    tests = [
      'test.1',
      'test.2',
    ]
    expected = '-test.1:test.2'

    self.assertEqual(test_apps.get_gtest_filter(tests, invert=True), expected)


if __name__ == '__main__':
  unittest.main()