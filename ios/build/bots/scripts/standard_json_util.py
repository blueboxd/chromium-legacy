# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from collections import OrderedDict

LOGGER = logging.getLogger(__name__)


class StdJson():

  def __init__(self, **kwargs):
    """Module for storing the results in standard JSON format.

    https://chromium.googlesource.com/chromium/src/+/master/docs/testing/json_test_results_format.md
    """

    self.tests = OrderedDict()

    if 'passed' in kwargs:
      self.mark_all_passed(kwargs['passed'])
    if 'failed' in kwargs:
      self.mark_all_failed(kwargs['failed'])
    if 'flaked' in kwargs:
      self.mark_all_passed(kwargs['flaked'], flaky=True)

  def mark_passed(self, test, flaky=False):
    """Set test as passed

    Params:
      test (str): a test in format "{TestCase}/{testMethod}"

    If flaky=True, or if 'FAIL' already set in 'actual',
    apply is_flaky=True for all test(s).
    """
    if not test:
      LOGGER.warn('Empty or None test name passed to standard_json_util')
      return

    if test in self.tests:
      self.tests[test]['actual'] = self.tests[test]['actual'] + " PASS"
    else:
      self.tests[test] = {'expected': 'PASS', 'actual': 'PASS'}

    if flaky or 'FAIL' in self.tests[test]['actual']:
      self.tests[test]['is_flaky'] = True

    self.tests[test].pop('is_unexpected', None)

  def mark_all_passed(self, tests, flaky=False):
    """Mark all tests as PASS"""
    for test in tests:
      self.mark_passed(test, flaky)

  def mark_failed(self, test):
    """Set test(s) as failed.

    Params:
      test (str): a test in format "{TestCase}/{testMethod}"
    """
    if not test:
      LOGGER.warn('Empty or None test name passed to standard_json_util')
      return

    if test in self.tests:
      self.tests[test]['actual'] = self.tests[test]['actual'] + " FAIL"
      self.tests[test]['is_unexpected'] = True
    else:
      self.tests[test] = {
          'expected': 'PASS',
          'actual': 'FAIL',
          'is_unexpected': True
      }

  def mark_all_failed(self, tests):
    """Mark all tests as FAIL"""
    for test in tests:
      self.mark_failed(test)

  def mark_skipped(self, test):
    """Set test(s) as SKIP.

    Params:
      test (str): a test in format "{TestCase}/{testMethod}"
    """
    if not test:
      LOGGER.warn('Empty or None test name passed to standard_json_util')
      return

    self.tests[test] = {'expected': 'SKIP', 'actual': 'SKIP'}

  def mark_all_skipped(self, tests):
    for test in tests:
      self.mark_skipped(test)

  def mark_timeout(self, test):
    """Set test as TIMEOUT, which is used to indicate a test abort/timeout

    Params:
      test (str): a test in format "{TestCase}/{testMethod}"
    """
    if not test:
      LOGGER.warn('Empty or None test name passed to standard_json_util')
      return

    if test in self.tests:
      self.tests[test]['actual'] = self.tests[test]['actual'] + " TIMEOUT"
      self.tests[test]['is_unexpected'] = True
    else:
      self.tests[test] = {
          'expected': 'PASS',
          'actual': 'TIMEOUT',
          'is_unexpected': True
      }
