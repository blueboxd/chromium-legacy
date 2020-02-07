# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mock import patch
import os
import shard_util
import unittest

DEBUG_APP_OTOOL_OUTPUT = '\n'.join([
    'Meta Class', 'name 0x1064b8438 CacheTestCase',
    'baseMethods 0x1068586d8 (struct method_list_t *)',
    'imp 0x1075e6887 -[CacheTestCase testA]', 'types 0x1064cc3e1',
    'imp 0x1075e6887 -[CacheTestCase testB]',
    'imp 0x1075e6887 -[CacheTestCase testc]', 'name 0x1064b8438 TabUITestCase',
    'baseMethods 0x1068586d8 (struct method_list_t *)',
    'imp 0x1075e6887 -[TabUITestCase testD]', 'types 0x1064cc3e1 v16@0:8',
    'imp 0x1075e6887 -[TabUITestCase testE]',
    'name 0x1064b8438 KeyboardTestCase',
    'imp 0x1075e6887 -[KeyboardTestCase testF]',
    'name 0x1064b8438 PasswordsTestCase',
    'imp 0x1075e6887 -[PasswordsTestCase testG]',
    'name 0x1064b8438 ToolBarTestCase',
    'imp 0x1075e6887 -[ToolBarTestCase testH]', 'version 0'
])

RELEASE_APP_OTOOL_OUTPUT = '\n'.join([
    'Meta Class', 'name 0x1064b8438 CacheTestCase',
    'baseMethods 0x1068586d8 (struct method_list_t *)',
    'name 0x1075e6887 testA', 'types 0x1064cc3e1', 'name 0x1075e6887 testB',
    'name 0x1075e6887 testc', 'baseProtocols 0x0', 'Meta Class',
    'name 0x1064b8438 TabUITestCase',
    'baseMethods 0x1068586d8 (struct method_list_t *)',
    'name 0x1064b8438 KeyboardTest', 'name 0x1075e6887 testD',
    'types 0x1064cc3e1 v16@0:8', 'name 0x1075e6887 testE',
    'name 0x1075e6887 testF', 'baseProtocols 0x0',
    'name 0x1064b8438 ChromeTestCase', 'name 0x1064b8438 setUp',
    'baseProtocols 0x0', 'name 0x1064b8438 ToolBarTestCase',
    'name 0x1075e6887 testG', 'name 0x1075e6887 testH', 'baseProtocols 0x0',
    'version 0'
])


class TestShardUtil(unittest.TestCase):
  """Test cases for shard_util.py"""

  @patch('shard_util.os.path.abspath')
  def test_determine_path_non_eg2(self, mock_abspath):
    mock_abspath.return_value = '/b/s/w/ir/ios/build/bots/scripts/share_util.py'
    app = 'some_ios_test.app'

    actual_path = shard_util.determine_app_path(app)
    expected_path = os.path.join('/b/s/w/ir/ios/build/bots/scripts',
                                 '../../../..', 'out/Debug', app,
                                 'some_ios_test')
    self.assertEqual(actual_path, expected_path)

  @patch('shard_util.os.path.abspath')
  def test_determine_path_eg2(self, mock_abspath):
    mock_abspath.return_value = '/b/s/w/ir/ios/build/bots/scripts/share_util.py'
    app = 'some_ios_test-Runner.app'
    host = 'some_host.app'

    actual_path = shard_util.determine_app_path(app, host)
    expected_path = os.path.join('/b/s/w/ir/ios/build/bots/scripts',
                                 '../../../..', 'out/Debug', app, 'PlugIns',
                                 'some_ios_test.xctest', 'some_ios_test')
    self.assertEqual(actual_path, expected_path)

  def test_fetch_debug_ok(self):
    """Ensures that the debug output is formatted correctly"""
    resp = shard_util.fetch_counts_for_debug(DEBUG_APP_OTOOL_OUTPUT)
    self.assertEqual(len(resp), 5)

    # ({'CacheTestCase': 3, 'TabUITestCase': 2, 'PasswordsTestCase': 1,
    # 'KeyboardTestCase': 1, 'ToolBarTestCase': 1})
    counts = resp.most_common()
    name, _ = counts[0]
    self.assertEqual(name, 'CacheTestCase')

  def test_fetch_release_ok(self):
    """Ensures that the release output is formatted correctly"""
    resp = shard_util.fetch_counts_for_release(RELEASE_APP_OTOOL_OUTPUT)
    self.assertEqual(len(resp), 4)

    # ({'KeyboardTest': 3, 'CacheTestCase': 3,
    # 'ToolBarTestCase': 2, 'TabUITestCase': 0})
    counts = resp.most_common()
    name, _ = counts[0]
    self.assertEqual(name, 'KeyboardTest')

  def test_fetch_test_counts_debug(self):
    """Ensure the coordinator functions as designed, for release"""
    test_counts = shard_util.fetch_test_counts(DEBUG_APP_OTOOL_OUTPUT, False)
    self.assertEqual(len(test_counts), 5)

  def test_fetch_test_counts_release(self):
    """Ensure the coordinator functions as designed, for release"""
    test_counts = shard_util.fetch_test_counts(RELEASE_APP_OTOOL_OUTPUT, True)
    self.assertEqual(len(test_counts), 4)

  def test_balance_into_sublists_debug(self):
    """Ensure the balancing algorithm works"""
    test_counts = shard_util.fetch_test_counts(DEBUG_APP_OTOOL_OUTPUT, False)

    sublists_1 = shard_util.balance_into_sublists(test_counts, 1)
    self.assertEqual(len(sublists_1), 1)
    self.assertEqual(len(sublists_1[0]), 5)

    sublists_3 = shard_util.balance_into_sublists(test_counts, 3)
    self.assertEqual(len(sublists_3), 3)
    # CacheTestCase has 3,
    # TabUITestCase has 2, ToolBarTestCase has 1
    # PasswordsTestCase has 1, KeyboardTestCase has 1
    self.assertEqual(len(sublists_3[0]), 1)
    self.assertEqual(len(sublists_3[1]), 2)
    self.assertEqual(len(sublists_3[2]), 2)

  def test_balance_into_sublists_release(self):
    """Ensure the balancing algorithm works"""
    test_counts = shard_util.fetch_test_counts(RELEASE_APP_OTOOL_OUTPUT, True)

    sublists_3 = shard_util.balance_into_sublists(test_counts, 3)
    self.assertEqual(len(sublists_3), 3)
    # KeyboardTest has 3
    # CacheTestCase has 3
    # ToolbarTest Case has 2, TabUITestCase has 0
    self.assertEqual(len(sublists_3[0]), 1)
    self.assertEqual(len(sublists_3[1]), 1)
    self.assertEqual(len(sublists_3[2]), 2)


if __name__ == '__main__':
  unittest.main()
