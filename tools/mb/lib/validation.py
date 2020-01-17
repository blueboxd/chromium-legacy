# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Validation functions for the Meta-Build config file"""

import collections


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
        args = flatten_config(config_pool, mixin_pool, config)['gn_args']
        if 'error' in args:
          continue

      evaled_to_source[args].add(config)

  for v in evaled_to_source.values():
    if len(v) != 1:
      errs.append(
          'Duplicate configs detected. When evaluated fully, the '
          'following configs are all equivalent: %s. Please '
          'consolidate these configs into only one unique name per '
          'configuration value.' % (', '.join(sorted('%r' % val for val in v))))
