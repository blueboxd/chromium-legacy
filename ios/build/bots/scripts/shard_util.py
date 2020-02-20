# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import os
import re
import subprocess
import test_runner as tr

LOGGER = logging.getLogger(__name__)

# WARNING: THESE DUPLICATE CONSTANTS IN:
# //build/scripts/slave/recipe_modules/ios/api.py

TEST_NAMES_DEBUG_APP_PATTERN = re.compile(
    'imp (?:0[xX][0-9a-fA-F]+ )?-\[(?P<testSuite>[A-Za-z_][A-Za-z0-9_]'
    '*Test[Case]*) (?P<testMethod>test[A-Za-z0-9_]*)\]')
TEST_CLASS_RELEASE_APP_PATTERN = re.compile(
    r'name 0[xX]\w+ '
    '(?P<testSuite>[A-Za-z_][A-Za-z0-9_]*Test(?:Case|))\n')
TEST_NAME_RELEASE_APP_PATTERN = re.compile(
    r'name 0[xX]\w+ (?P<testCase>test[A-Za-z0-9_]+)\n')
# 'ChromeTestCase' and 'BaseEarlGreyTestCase' are parent classes
# of all EarlGrey/EarlGrey2 test classes. They have no real tests.
IGNORED_CLASSES = ['BaseEarlGreyTestCase', 'ChromeTestCase']


def determine_app_path(app, host_app=None):
  """String manipulate args.app and args.host to determine what path to use
    for otools

    Args:
        app: (string) args.app
        host_app: (string) args.host_app

    Returns:
        (string) path to app for otools to analyze
    """
  # run.py invoked via ../../ios/build/bots/scripts/, so we reverse this
  dirname = os.path.dirname(os.path.abspath(__file__))
  # location of app: /b/s/w/ir/out/Debug/test.app
  full_app_path = os.path.join(dirname, '../../../..', 'out/Debug', app)

  # ie/ if app_path = "../../some.app", app_name = some
  start_idx = 0
  if '/' in app:
    start_idx = app.rindex('/')
  app_name = app[start_idx:app.rindex('.app')]

  # Default app_path looks like /b/s/w/ir/out/Debug/test.app/test
  app_path = os.path.join(full_app_path, app_name)

  if host_app and host_app != 'NO_PATH':
    LOGGER.debug("Detected EG2 test while building application path. "
                 "Host app: {}".format(host_app))
    # EG2 tests always end in -Runner, so we split that off
    app_name = app[:app.rindex('-Runner')]
    app_path = os.path.join(full_app_path, 'PlugIns',
                            '{}.xctest'.format(app_name), app_name)

  return app_path


def _execute(cmd):
  """Helper for executing a command."""
  LOGGER.info('otool command: {}'.format(cmd))
  process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
  stdout = process.communicate()[0]

  retcode = process.returncode
  LOGGER.info('otool return status code: {}'.format(retcode))
  if retcode:
    raise tr.OtoolError(retcode)

  return stdout


def fetch_counts_for_release(stdout):
  """Invoke otools to determine the number of test case methods

     WARNING: This logic is a duplicate of what's found in
      //build/scripts/slave/recipe_modules/ios/api.py

    Args:
        stdout: (string) response of 'otool -ov'

    Returns:
        (collections.Counter) dict of test case to number of test case methods
    """
  # For Release builds `otool -ov` command generates output that is
  # different from Debug builds.
  # Parsing implemented in such a way:
  # 1. Parse test class names.
  # 2. If they are not in ignored list, parse test method names.
  # 3. Calculate test count per test class.
  test_counts = {}
  res = re.split(TEST_CLASS_RELEASE_APP_PATTERN, stdout)
  # Ignore 1st element in split since it does not have any test class data
  test_classes_output = res[1:]
  for test_class, class_output in zip(test_classes_output[0::2],
                                      test_classes_output[1::2]):
    if test_class in IGNORED_CLASSES:
      continue
    names = TEST_NAME_RELEASE_APP_PATTERN.findall(class_output)
    test_counts[test_class] = test_counts.get(test_class, 0) + len(set(names))

  return collections.Counter(test_counts)


def fetch_counts_for_debug(stdout):
  """Invoke otools to determine the number of test case methods

    Args:
        stdout: (string) response of 'otool -ov'

    Returns:
        (collections.Counter) dict of test case to number of test case methods
    """
  test_names = TEST_NAMES_DEBUG_APP_PATTERN.findall(stdout)
  test_counts = collections.Counter(test_class for test_class, _ in test_names
                                    if test_class not in IGNORED_CLASSES)

  return test_counts


def fetch_test_counts(stdout, release=False):
  """Determine the number of test case methods per test class.

    Args:
        app_path: (string) path to app

    Returns:
        (collections.Counter) dict of test_case to number of test case methods
    """
  LOGGER.info("Ignored test classes: {}".format(IGNORED_CLASSES))
  if release:
    LOGGER.info("Release build detected. Fetching count for release.")
    return fetch_counts_for_release(stdout)

  return fetch_counts_for_debug(stdout)


def balance_into_sublists(test_counts, total_shards):
  """Augment the result of otool into balanced sublists

  Args:
    test_counts: (collections.Counter) dict of test_case to test case numbers
    total_shards: (int) total number of shards this was divided into

  Returns:
    list of list of test classes
  """

  class Shard(object):
    """Stores list of test classes and number of all tests"""

    def __init__(self):
      self.test_classes = []
      self.size = 0

  shards = [Shard() for i in range(total_shards)]

  # Balances test classes between shards to have
  # approximately equal number of tests per shard.
  for test_class, number_of_test_methods in test_counts.most_common():
    min_shard = min(shards, key=lambda shard: shard.size)
    min_shard.test_classes.append(test_class)
    min_shard.size += number_of_test_methods

  sublists = [shard.test_classes for shard in shards]
  return sublists


def shard_test_cases(args, shard_index, total_shards):
  """Shard test cases into total_shards, and determine which test cases to
    run for this shard.

    Args:
        args: all parsed arguments passed to run.py
        shard_index: the shard index(number) for this run
        total_shards: the total number of shards for this test

    Returns: a list of test cases to execute
    """
  # Convert to dict format
  dict_args = vars(args)
  app = dict_args['app']
  host_app = dict_args.get('host_app', None)

  # Determine what path to use
  app_path = determine_app_path(app, host_app)

  # release argument is passed by MB only when is_debug=False.
  # 'Release' can also be set in path for this file (out/Release), so we'll
  # check for either.
  release = (
      dict_args.get('release', False) or 'Release' in os.path.abspath(__file__))

  # Use otools to get the test counts
  cmd = ['otool', '-ov', app_path]
  stdout = _execute(cmd)

  test_counts = fetch_test_counts(stdout, release)

  # Ensure shard and total shard is int
  shard_index = int(shard_index)
  total_shards = int(total_shards)

  sublists = balance_into_sublists(test_counts, total_shards)
  tests = sublists[shard_index]

  LOGGER.info("Tests to be executed this round: {}".format(tests))
  return tests
