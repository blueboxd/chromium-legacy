# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Validation functions for the Meta-Build config file"""

import ast
import collections
import json
import re


def GetAllConfigsMaster(masters):
  """Build a list of all of the configs referenced by builders.

  Deprecated in favor or GetAllConfigsBucket
  """
  all_configs = {}
  for master in masters:
    for config in masters[master].values():
      if isinstance(config, dict):
        for c in config.values():
          all_configs[c] = master
      else:
        all_configs[config] = master
  return all_configs


def GetAllConfigsBucket(buckets):
  """Build a list of all of the configs referenced by builders."""
  all_configs = {}
  for bucket in buckets:
    for config in buckets[bucket].values():
      if isinstance(config, dict):
        for c in config.values():
          all_configs[c] = bucket
      else:
        all_configs[config] = bucket
  return all_configs


def CheckAllConfigsAndMixinsReferenced(errs, all_configs, configs, mixins):
  """Check that every actual config is actually referenced."""
  for config in configs:
    if not config in all_configs:
      errs.append('Unused config "%s".' % config)

  # Figure out the whole list of mixins, and check that every mixin
  # listed by a config or another mixin actually exists.
  referenced_mixins = set()
  for config, mixin_names in configs.items():
    for mixin in mixin_names:
      if not mixin in mixins:
        errs.append(
            'Unknown mixin "%s" referenced by config "%s".' % (mixin, config))
      referenced_mixins.add(mixin)

  for mixin in mixins:
    for sub_mixin in mixins[mixin].get('mixins', []):
      if not sub_mixin in mixins:
        errs.append(
            'Unknown mixin "%s" referenced by mixin "%s".' % (sub_mixin, mixin))
      referenced_mixins.add(sub_mixin)

  # Check that every mixin defined is actually referenced somewhere.
  for mixin in mixins:
    if not mixin in referenced_mixins:
      errs.append('Unreferenced mixin "%s".' % mixin)

  return errs


def EnsureNoProprietaryMixinsBucket(errs, default_config, config_file,
                                    public_artifact_builders, buckets, configs,
                                    mixins):
  """Check that the 'chromium' bots which build public artifacts
  do not include the chrome_with_codecs mixin.
  """
  if config_file != default_config:
    return

  if public_artifact_builders is None:
    errs.append('Missing "public_artifact_builders" config entry. '
                'Please update this proprietary codecs check with the '
                'name of the builders responsible for public build artifacts.')
    return

  # crbug/1033585
  for bucket, builders in public_artifact_builders.items():
    for builder in builders:
      config = buckets[bucket][builder]

      def RecurseMixins(builder, current_mixin):
        if current_mixin == 'chrome_with_codecs':
          errs.append('Public artifact builder "%s" can not contain the '
                      '"chrome_with_codecs" mixin.' % builder)
          return
        if not 'mixins' in mixins[current_mixin]:
          return
        for mixin in mixins[current_mixin]['mixins']:
          RecurseMixins(builder, mixin)

      for mixin in configs[config]:
        RecurseMixins(builder, mixin)

  return errs


def EnsureNoProprietaryMixinsMaster(errs, default_config, config_file, masters,
                                    configs, mixins):
  """If we're checking the Chromium config, check that the 'chromium' bots
  which build public artifacts do not include the chrome_with_codecs mixin.

  Deprecated in favor of BlacklistMixinsBucket
  """
  if config_file == default_config:
    if 'chromium' in masters:
      for builder in masters['chromium']:
        config = masters['chromium'][builder]

        def RecurseMixins(current_mixin):
          if current_mixin == 'chrome_with_codecs':
            errs.append('Public artifact builder "%s" can not contain the '
                        '"chrome_with_codecs" mixin.' % builder)
            return
          if not 'mixins' in mixins[current_mixin]:
            return
          for mixin in mixins[current_mixin]['mixins']:
            RecurseMixins(mixin)

        for mixin in configs[config]:
          RecurseMixins(mixin)
    else:
      errs.append('Missing "chromium" master. Please update this '
                  'proprietary codecs check with the name of the master '
                  'responsible for public build artifacts.')


def _GetConfigsByBuilder(masters):
  """Builds a mapping from buildername -> [config]

    Args
      masters: the master's dict from mb_config.pyl
    """

  result = collections.defaultdict(list)
  for master in masters.values():
    for buildername, builder in master.items():
      result[buildername].append(builder)

  return result


def CheckMasterBucketConsistency(errs, masters_file, buckets_file):
  """Checks that mb_config_buckets.pyl is consistent with mb_config.pyl

    mb_config_buckets.pyl is a subset of mb_config.pyl.
    Make sure all configs that do exist are consistent.
    Populates errs with any errors

    Args:
      errs: an accumulator for errors
      masters_file: string form of mb_config.pyl
      bucket_file: string form of mb_config_buckets.pyl
    """
  master_contents = ast.literal_eval(masters_file)
  bucket_contents = ast.literal_eval(buckets_file)

  def check_missing(bucket_dict, master_dict):
    return [name for name in bucket_dict.keys() if name not in master_dict]

  # Cross check builders
  configs_by_builder = _GetConfigsByBuilder(master_contents['masters'])
  for bucketname, builders in bucket_contents['buckets'].items():
    missing = check_missing(builders, configs_by_builder)
    errs.extend('Builder "%s" in bucket "%s" from mb_config_buckets.pyl '
                'not found in mb_config.pyl' % (builder, bucketname)
                for builder in missing)

    for buildername, config in builders.items():
      if config not in configs_by_builder[buildername]:
        errs.append('Builder "%s" in bucket "%s" from mb_config_buckets.pyl '
                    'doesn\'t match mb_config.pyl' % (buildername, bucketname))

  def check_mismatch(bucket_dict, master_dict):
    mismatched = []
    for configname, config in bucket_dict.items():
      if configname in master_dict and config != master_dict[configname]:
        mismatched.append(configname)

    return mismatched

  # Cross check configs
  missing = check_missing(bucket_contents['configs'],
                          master_contents['configs'])
  errs.extend('Config "%s" from mb_config_buckets.pyl '
              'not found in mb_config.pyl' % config for config in missing)

  mismatched = check_mismatch(bucket_contents['configs'],
                              master_contents['configs'])
  errs.extend(
      'Config "%s" from mb_config_buckets.pyl doesn\'t match mb_config.pyl' %
      config for config in mismatched)

  # Cross check mixins
  missing = check_missing(bucket_contents['mixins'], master_contents['mixins'])
  errs.extend('Mixin "%s" from mb_config_buckets.pyl '
              'not found in mb_config.pyl' % mixin for mixin in missing)

  mismatched = check_mismatch(bucket_contents['mixins'],
                              master_contents['mixins'])
  errs.extend(
      'Mixin "%s" from mb_config_buckets.pyl doesn\'t match mb_config.pyl' %
      mixin for mixin in mismatched)


def CheckDuplicateConfigs(errs, config_pool, mixin_pool, grouping,
                          flatten_config):
  """Check for duplicate configs.

  Evaluate all configs, and see if, when
  evaluated, differently named configs are the same.
  """
  evaled_to_source = collections.defaultdict(set)
  for group, builders in grouping.items():
    for builder in builders:
      config = grouping[group][builder]
      if not config:
        continue

      if isinstance(config, dict):
        # Ignore for now
        continue
      elif config.startswith('//'):
        args = config
      else:
        flattened_config = flatten_config(config_pool, mixin_pool, config)
        args = flattened_config['gn_args']
        if 'error' in args:
          continue
        # Force the args_file into consideration when testing for duplicate
        # configs.
        args_file = flattened_config['args_file']
        if args_file:
          args += ' args_file=%s' % args_file

      evaled_to_source[args].add(config)

  for v in evaled_to_source.values():
    if len(v) != 1:
      errs.append(
          'Duplicate configs detected. When evaluated fully, the '
          'following configs are all equivalent: %s. Please '
          'consolidate these configs into only one unique name per '
          'configuration value.' % (', '.join(sorted('%r' % val for val in v))))
