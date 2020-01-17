#!/usr/bin/python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for mb_validate.py."""

from __future__ import print_function
from __future__ import absolute_import

import sys
import ast
import os
import unittest

sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..'))

from mb import mb
from mb import mb_unittest
from mb.lib import validation

TEST_UNREFERENCED_MIXIN_CONFIG = """\
{
  'public_artifact_builders': {},
  'configs': {
    'rel_bot_1': ['rel'],
    'rel_bot_2': ['rel'],
  },
  'buckets': {
    'fake_bucket_a': {
      'fake_builder_a': 'rel_bot_1',
      'fake_builder_b': 'rel_bot_2',
    },
  },
  'mixins': {
    'unreferenced_mixin': {
      'gn_args': 'proprietary_codecs=true',
    },
    'rel': {
      'gn_args': 'is_debug=false',
    },
  },
}
"""

TEST_UNKNOWNMIXIN_CONFIG = """\
{
  'public_artifact_builders': {},
  'configs': {
    'rel_bot_1': ['rel'],
    'rel_bot_2': ['rel', 'unknown_mixin'],
  },
  'buckets': {
    'fake_bucket_a': {
      'fake_builder_a': 'rel_bot_1',
      'fake_builder_b': 'rel_bot_2',
    },
  },
  'mixins': {
    'rel': {
      'gn_args': 'is_debug=false',
    },
  },
}
"""

TEST_UNKNOWN_NESTED_MIXIN_CONFIG = """\
{
  'public_artifact_builders': {},
  'configs': {
    'rel_bot_1': ['rel', 'nested_mixin'],
    'rel_bot_2': ['rel'],
  },
  'buckets': {
    'fake_bucket_a': {
      'fake_builder_a': 'rel_bot_1',
      'fake_builder_b': 'rel_bot_2',
    },
  },
  'mixins': {
    'nested_mixin': {
      'mixins': {
        'unknown_mixin': {
          'gn_args': 'proprietary_codecs=true',
        },
      },
    },
    'rel': {
      'gn_args': 'is_debug=false',
    },
  },
}
"""


class UnitTest(unittest.TestCase):
  def test_GetAllConfigsMaster(self):
    configs = ast.literal_eval(mb_unittest.TEST_CONFIG)
    all_configs = validation.GetAllConfigsMaster(configs['masters'])
    self.assertEqual(all_configs['rel_bot'], 'fake_master')
    self.assertEqual(all_configs['debug_goma'], 'fake_master')

  def test_GetAllConfigsBucket(self):
    configs = ast.literal_eval(mb_unittest.TEST_CONFIG_BUCKET)
    all_configs = validation.GetAllConfigsBucket(configs['buckets'])
    self.assertEqual(all_configs['rel_bot'], 'ci')
    self.assertEqual(all_configs['debug_goma'], 'ci')

  def test_CheckAllConfigsAndMixinsReferenced_ok(self):
    configs = ast.literal_eval(mb_unittest.TEST_CONFIG_BUCKET)
    errs = []
    all_configs = validation.GetAllConfigsBucket(configs['buckets'])
    config_configs = configs['configs']
    mixins = configs['mixins']

    validation.CheckAllConfigsAndMixinsReferenced(errs, all_configs,
                                                  config_configs, mixins)

    self.assertEqual(errs, [])

  def test_CheckAllConfigsAndMixinsReferenced_unreferenced(self):
    configs = ast.literal_eval(TEST_UNREFERENCED_MIXIN_CONFIG)
    errs = []
    all_configs = validation.GetAllConfigsMaster(configs['buckets'])
    config_configs = configs['configs']
    mixins = configs['mixins']

    validation.CheckAllConfigsAndMixinsReferenced(errs, all_configs,
                                                  config_configs, mixins)

    self.assertIn('Unreferenced mixin "unreferenced_mixin".', errs)

  def test_CheckAllConfigsAndMixinsReferenced_unknown(self):
    configs = ast.literal_eval(TEST_UNKNOWNMIXIN_CONFIG)
    errs = []
    all_configs = validation.GetAllConfigsMaster(configs['buckets'])
    config_configs = configs['configs']
    mixins = configs['mixins']

    validation.CheckAllConfigsAndMixinsReferenced(errs, all_configs,
                                                  config_configs, mixins)
    self.assertIn(
        'Unknown mixin "unknown_mixin" '
        'referenced by config "rel_bot_2".', errs)

  def test_CheckAllConfigsAndMixinsReferenced_unknown_nested(self):
    configs = ast.literal_eval(TEST_UNKNOWN_NESTED_MIXIN_CONFIG)
    errs = []
    all_configs = validation.GetAllConfigsMaster(configs['buckets'])
    config_configs = configs['configs']
    mixins = configs['mixins']

    validation.CheckAllConfigsAndMixinsReferenced(errs, all_configs,
                                                  config_configs, mixins)

    self.assertIn(
        'Unknown mixin "unknown_mixin" '
        'referenced by mixin "nested_mixin".', errs)

  def test_CheckAllConfigsAndMixinsReferenced_unused(self):
    configs = ast.literal_eval(TEST_UNKNOWN_NESTED_MIXIN_CONFIG)
    errs = []
    all_configs = validation.GetAllConfigsMaster(configs['buckets'])
    config_configs = configs['configs']
    mixins = configs['mixins']

    validation.CheckAllConfigsAndMixinsReferenced(errs, all_configs,
                                                  config_configs, mixins)

    self.assertIn(
        'Unknown mixin "unknown_mixin" '
        'referenced by mixin "nested_mixin".', errs)

  def test_EnsureNoProprietaryMixinsBucket(self):
    bad_configs = ast.literal_eval(mb_unittest.TEST_BAD_CONFIG_BUCKET)
    errs = []
    default_config = 'fake_config_file'
    config_file = 'fake_config_file'
    public_artifact_builders = bad_configs['public_artifact_builders']
    buckets = bad_configs['buckets']
    mixins = bad_configs['mixins']
    config_configs = bad_configs['configs']

    validation.EnsureNoProprietaryMixinsBucket(
        errs, default_config, config_file, public_artifact_builders, buckets,
        config_configs, mixins)

    self.assertIn(
        'Public artifact builder "fake_builder_a" '
        'can not contain the "chrome_with_codecs" mixin.', errs)
    self.assertIn(
        'Public artifact builder "fake_builder_b" '
        'can not contain the "chrome_with_codecs" mixin.', errs)
    self.assertEqual(len(errs), 2)

  def test_EnsureNoProprietaryMixinsMaster(self):
    bad_configs = ast.literal_eval(mb_unittest.TEST_BAD_CONFIG)
    errs = []
    default_config = 'fake_config_file'
    config_file = 'fake_config_file'
    buckets = bad_configs['masters']
    mixins = bad_configs['mixins']
    config_configs = bad_configs['configs']

    validation.EnsureNoProprietaryMixinsMaster(
        errs, default_config, config_file, buckets, config_configs, mixins)

    self.assertIn(
        'Public artifact builder "a" '
        'can not contain the "chrome_with_codecs" mixin.', errs)
    self.assertIn(
        'Public artifact builder "b" '
        'can not contain the "chrome_with_codecs" mixin.', errs)
    self.assertEqual(len(errs), 2)

  def test_CheckDuplicateConfigs_ok(self):
    configs = ast.literal_eval(mb_unittest.TEST_CONFIG_BUCKET)
    config_configs = configs['configs']
    mixins = configs['mixins']
    grouping = configs['buckets']
    errs = []

    validation.CheckDuplicateConfigs(errs, config_configs, mixins, grouping,
                                     mb.FlattenConfig)
    self.assertEqual(errs, [])

  @unittest.skip('bla')
  def test_CheckDuplicateConfigs_dups(self):
    configs = ast.literal_eval(mb_unittest.TEST_DUP_CONFIG_BUCKET)
    config_configs = configs['configs']
    mixins = configs['mixins']
    grouping = configs['buckets']
    errs = []

    validation.CheckDuplicateConfigs(errs, config_configs, mixins, grouping,
                                     mb.FlattenConfig)
    self.assertIn(
        'Duplicate configs detected. When evaluated fully, the '
        'following configs are all equivalent: \'some_config\', '
        '\'some_other_config\'. Please consolidate these configs '
        'into only one unique name per configuration value.', errs)


if __name__ == '__main__':
  unittest.main()
