#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import collections
import csv
import filecmp
import json
import multiprocessing
import os
import shutil
import sys
import tempfile
import textwrap

from core import path_util
path_util.AddTelemetryToPath()

from core import bot_platforms
from core import retrieve_story_timing
from core import sharding_map_generator

_SCRIPT_USAGE = """
Generate sharding maps for Telemetry benchmarks.

Every performance benchmark should be run on a same machine as long as possible
to preserve high fidelity of data monitoring. Hence in order to shard the
Telemetry benchmarks on multiple machines, we generate a JSON map that
specifies how benchmarks should be distributed on machines. There is one
sharding JSON map for every builder in the perf & perf.fyi waterfalls which are
specified by PerfPlatform classes in //tools/perf/core/bot_platforms.py.

Generating these JSON maps depends on how many Telemetry benchmarks
actually exist at the time. Because of this, CLs to generate the JSON maps
should never be automatically reverted, since the reverted state of the JSON map
files may not match with the true state of world.

"""

_BENCHMARK_SCHEDULES = 'tools/perf/benchmark_schedules.csv'
_CYCLETIME_CONTRIBUTION = 'tools/perf/cycletime_contributions.csv'


def GetParser():
  parser = argparse.ArgumentParser(
      description=_SCRIPT_USAGE, formatter_class=argparse.RawTextHelpFormatter)
  subparsers = parser.add_subparsers()

  parser_update = subparsers.add_parser(
      'update',
      help='Update the shard maps.')
  parser_update.add_argument(
      '--use-existing-timing-data', '-o', action='store_true',
      help=('Whether to reuse existing builder timing data (stored in '
            '//tools/perf/core/shard_maps/timing_data/) and skip the step of '
            'fetching the most recent timing data from test results server. '
            'This flag is default to False. One typically uses this option '
            'when they need to fix the timing data to debug sharding '
            'generation.'),
      default=False)
  _AddBuilderPlatformSelectionArgs(parser_update)
  parser.add_argument(
      '--debug', action='store_true',
      help=('Whether to include detailed debug info of the sharding map in the '
            'shard maps.'), default=False)
  parser_update.set_defaults(func=_UpdateShardsForBuilders)

  parser_update_timing = subparsers.add_parser(
      'update-timing',
      help='Update the timing data that is used to create the shard maps, '
      'but don\'t update the shard maps themselves.')
  _AddBuilderPlatformSelectionArgs(parser_update_timing)
  parser_update_timing.add_argument(
      '--filter-only', action='store_true',
      help='Do not grab new data from bigquery but instead simply filter '
      'the existing data to reflect some change in the benchmark (for example '
      'if the benchmark was switched to abridged mode on some platform or if '
      'a story was removed from the benchmark.)')
  parser_update_timing.set_defaults(func=_UpdateTimingDataCommand)

  parser_deschedule = subparsers.add_parser(
      'deschedule',
      help=('After you deschedule one or more '
            'benchmarks by deleting from tools/perf/benchmarks or by editing '
            'bot_platforms.py, use this script to deschedule the '
            'benchmark(s) without impacting the sharding for other benchmarks.'
            ))
  parser_deschedule.set_defaults(func=_DescheduleBenchmark)

  parser_validate = subparsers.add_parser(
      'validate',
      help=('Validate that the shard maps match up with the benchmarks and '
            'bot_platforms.py.'))
  parser_validate.set_defaults(func=_ValidateShardMaps)

  return parser


def _AddBuilderPlatformSelectionArgs(parser):
  builder_selection = parser.add_mutually_exclusive_group()
  builder_selection.add_argument(
      '--builders', '-b', action='append',
      help=('The builder names to use.'), default=[],
      choices=bot_platforms.ALL_PLATFORM_NAMES)
  builder_selection.add_argument(
      '--waterfall', '-w', choices=['perf', 'perf-fyi', 'all'], default=None,
      help=('The name of waterfall whose builders should be used. If not '
            'specified, use all perf builders by default'))


def _DumpJson(data, output_path):
  with open(output_path, 'w') as output_file:
    json.dump(data, output_file, indent=4, separators=(',', ': '))


def _LoadTimingData(args):
  builder, timing_file_path = args
  data = retrieve_story_timing.FetchAverageStoryTimingData(
      configurations=[builder.name], num_last_days=5)
  for executable in builder.executables:
    data.append({unicode('duration'): unicode(
                    float(executable.estimated_runtime)),
                 unicode('name'): unicode(
                     executable.name + '/' + bot_platforms.GTEST_STORY_NAME)})
  _DumpJson(data, timing_file_path)
  print('Finished retrieving story timing data for %s' % repr(builder.name))


def _source_filepath(posix_path):
  return os.path.join(path_util.GetChromiumSrcDir(), *posix_path.split('/'))


class _BenchmarkUsageRow(object):
  def __init__(self, name, platforms, duration):
    self.name = name
    self.platforms = platforms
    self.duration = duration

  def Add(self, other):
    if not self.name:
      self.name = other.name
      self.platforms = other.platforms
      self.duration = other.duration
    else:
      assert self.name == other.name, '%s,%s' % (self.name, other.name)
      self.platforms |= other.platforms
      self.duration += other.duration

  def _GetBenchmarkConfiguration(self, platform):
    for benchmark_config in platform.benchmark_configs:
      if benchmark_config.name == self.name:
        return benchmark_config
    for executable in platform.executables:
      if executable.name == self.name:
        return executable

  def ToRow(self):
    unabridged_platforms = sorted([
        p.name for p in self.platforms
        if not self._GetBenchmarkConfiguration(p).abridged])
    abridged_platforms = sorted([
        p.name for p in self.platforms
        if self._GetBenchmarkConfiguration(p).abridged])
    unabridged_count = len(unabridged_platforms)
    abridged_count = len(abridged_platforms)
    platform_names = sorted([p.name for p in self.platforms])
    assert len(platform_names) == unabridged_count + abridged_count
    return [self.name, '%.2f' % (self.duration/60.0/60.0), unabridged_count,
            abridged_count, ', '.join(unabridged_platforms),
            ', '.join(abridged_platforms)]


def _UpdateWaterfallUsageCsvs(schedules_output_path=None,
                              cycletimes_output_path=None):
  schedules_output_path = schedules_output_path or _source_filepath(
      _BENCHMARK_SCHEDULES)
  cycletimes_output_path = cycletimes_output_path or _source_filepath(
      _CYCLETIME_CONTRIBUTION)
  builders = sorted(_GetBuilderPlatforms(builders=None, waterfall='perf'),
                    key=lambda b: b.name)
  # https://stackoverflow.com/questions/31838823
  EmptyBenchmarkUsageRow = lambda: _BenchmarkUsageRow(None, None, None)
  benchmark_waterfall_usage = collections.defaultdict(EmptyBenchmarkUsageRow)
  platforms = collections.OrderedDict()
  for builder in builders:
    platforms[builder] = collections.defaultdict(float)
    with open(builder.timing_file_path) as fh:
      timing_data = json.load(fh)
    for story in timing_data:
      benchmark_name = story['name'].split('/')[0]
      platforms[builder][benchmark_name] += float(story['duration'])
      benchmark_waterfall_usage[benchmark_name].Add(_BenchmarkUsageRow(
          benchmark_name, set((builder,)), float(story['duration'])))
  benchmarks = benchmark_waterfall_usage.values()
  benchmark_names = benchmark_waterfall_usage.keys()
  benchmarks.sort(key=lambda b: b.duration, reverse=True)
  _UpdateSchedulesCsv(benchmarks, schedules_output_path)
  _UpdateCycletimesCsv(platforms, benchmark_names, cycletimes_output_path)


def _UpdateCycletimesCsv(platforms, benchmark_names, output_path):
  benchmark_names = sorted(benchmark_names)
  csv_data = [
      ['AUTOGENERATED FILE DO NOT EDIT'],
      ['View a prettier version of this at',
       'https://docs.google.com/spreadsheets/d/'
       '15pJY4cxtM2NVNFKQDgDnoT5PLo0Nm5Td-Ov-5PZefAw']]
  columns = [
      ['platform',
       'shards',
       'idealized cycle time (hours)'] + benchmark_names]
  for platform, durations in platforms.iteritems():
    multiplier = 2.0 if platform.run_reference_build else 1.0
    estimated_cycle_time = (multiplier * sum(durations.values()) /
                            60.0 / 60.0 / float(platform.num_shards))
    # Double all cycle time stats by 2 to account for reference build.
    columns.append([
      platform.name,
      platform.num_shards,
      '%.2f' % estimated_cycle_time] + [
          '%.3f' % (multiplier * durations[name] / 60.0 /
                    60.0 / float(platform.num_shards))
          for name in benchmark_names]
    )
  # Rotate the columns into rows.
  rows = []
  for row_index in range(len(columns[0])):
    row = []
    # pylint: disable=consider-using-enumerate
    for column_index in range(len(columns)):
      row.append(columns[column_index][row_index])
    rows.append(row)
  csv_data.extend(rows)
  with open(output_path, 'w') as fh:
    writer = csv.writer(fh, lineterminator='\n')
    writer.writerows(csv_data)


def _UpdateSchedulesCsv(benchmark_rows, output_path):
  csv_data = [
      ['AUTOGENERATED FILE DO NOT EDIT'],
      ['View a prettier version of this at',
       'https://docs.google.com/spreadsheets/d/'
       '1RQepnii8sGTGiSdcQfWPpaYMZd6SRk9bRKd_HrdC3jI'],
      ['benchmark name',
       'total device usage hours per cycle',
       'platforms count (unabridged)',
       'platforms count (abridged)',
       'platforms where unabridged',
       'platforms where abridged']]
  for benchmark in benchmark_rows:
    csv_data.append(benchmark.ToRow())
  with open(output_path, 'w') as fh:
    writer = csv.writer(fh, lineterminator='\n')
    writer.writerows(csv_data)


def _GenerateShardMap(
    builder, num_of_shards, output_path, debug):
  timing_data = []
  if builder:
    with open(builder.timing_file_path) as f:
      timing_data = json.load(f)
  benchmarks_to_shard = (
      list(builder.benchmark_configs) + list(builder.executables))
  sharding_map = sharding_map_generator.generate_sharding_map(
      benchmarks_to_shard, timing_data,
      num_shards=num_of_shards,
      debug=debug)
  _DumpJson(sharding_map, output_path)


def _PromptWarning():
  message = ('This will regenerate the sharding maps for perf benchmarks. '
             'Note that this will shuffle all the benchmarks on the shards, '
             'which can cause false regressions. In general this operation '
             'should only be done when the shards are too unbalanced or when '
             'benchmarks are added/removed. '
             'In addition, this is a tricky operation and should '
             'always be reviewed by Benchmarking '
             'team members. Upon landing the CL to update the shard maps, '
             'please notify Chromium perf sheriffs in '
             'perf-sheriffs@chromium.org and put a warning about expected '
             'false regressions in your CL '
             'description')
  print(textwrap.fill(message, 70), '\n')
  answer = raw_input("Enter 'y' to continue: ")
  if answer != 'y':
    print('Abort updating shard maps for benchmarks on perf waterfall')
    sys.exit(0)


def _UpdateTimingDataCommand(args):
  builders = _GetBuilderPlatforms(args.builders, args.waterfall)
  if not args.filter_only:
    _UpdateTimingData(builders)
  for builder in builders:
    _FilterTimingData(builder)
  _UpdateWaterfallUsageCsvs()


def _FilterTimingData(builder, output_path=None):
  output_path = output_path or builder.timing_file_path
  with open(builder.timing_file_path) as f:
    timing_dataset = json.load(f)
  story_full_names = set()
  for benchmark_config in builder.benchmark_configs:
    for story in benchmark_config.stories:
      story_full_names.add('/'.join([benchmark_config.name, story]))
  # When benchmarks are abridged or stories are removed, we want that
  # to be reflected in the timing data right away.
  executable_story_names = [e.name + '/' + bot_platforms.GTEST_STORY_NAME
                            for e in builder.executables]
  timing_dataset = [point for point in timing_dataset
                    if (str(point['name']) in story_full_names or
                        str(point['name']) in executable_story_names)]
  _DumpJson(timing_dataset, output_path)


def _UpdateTimingData(builders):
  print('Updating shards timing data. May take a while...')
  load_timing_args = []
  for b in builders:
    load_timing_args.append((b, b.timing_file_path))
  p = multiprocessing.Pool(len(load_timing_args))
  # Use map_async to work around python bug. See crbug.com/1026004.
  p.map_async(_LoadTimingData, load_timing_args).get(12*60*60)


def _GetBuilderPlatforms(builders, waterfall):
  """Get a list of PerfBuilder objects for the given builders or waterfall.

  Otherwise, just return all platforms.
  """
  if builders:
    return {b for b in bot_platforms.ALL_PLATFORMS if b.name in
                builders}
  elif waterfall == 'perf':
    return bot_platforms.OFFICIAL_PLATFORMS
  elif waterfall == 'perf-fyi':
    return bot_platforms.FYI_PLATFORMS
  else:
    return bot_platforms.ALL_PLATFORMS


def _UpdateShardsForBuilders(args):
  _PromptWarning()
  builders = _GetBuilderPlatforms(args.builders, args.waterfall)
  if not args.use_existing_timing_data:
    _UpdateTimingData(builders)
  for b in builders:
    _GenerateShardMap(
        b, b.num_shards, b.shards_map_file_path, args.debug)
    print('Updated sharding map for %s' % repr(b.name))


def _DescheduleBenchmark(args):
  """Remove benchmarks from the shard maps without re-sharding."""
  del args
  builders = bot_platforms.ALL_PLATFORMS
  for b in builders:
    benchmarks_to_keep = set(
        benchmark.Name() for benchmark in b.benchmarks_to_run)
    with open(b.shards_map_file_path, 'r') as f:
      if not os.path.exists(b.shards_map_file_path):
        continue
      shards_map = json.load(f, object_pairs_hook=collections.OrderedDict)
      for shard, shard_map in shards_map.items():
        if shard == 'extra_infos':
          break
        benchmarks = shard_map['benchmarks']
        for benchmark in benchmarks.keys():
          if benchmark not in benchmarks_to_keep:
            del benchmarks[benchmark]
    os.remove(b.shards_map_file_path)
    _DumpJson(shards_map, b.shards_map_file_path)
  print('done.')


def _ParseBenchmarks(shard_map_path):
  if not os.path.exists(shard_map_path):
    raise RuntimeError(
        'Platform does not have a shard map at %s.' % shard_map_path)
  all_benchmarks = set()
  with open(shard_map_path) as f:
    shard_map = json.load(f)
  for shard, benchmarks_in_shard in shard_map.iteritems():
    if "extra_infos" in shard:
      continue
    if benchmarks_in_shard.get('benchmarks'):
      all_benchmarks |= set(benchmarks_in_shard['benchmarks'].keys())
    if benchmarks_in_shard.get('executables'):
      all_benchmarks |= set(benchmarks_in_shard['executables'].keys())
  return frozenset(all_benchmarks)


def _ValidateShardMaps(args):
  """Validate that the shard maps, csv files, etc. are consistent."""
  del args
  errors = []

  tempdir = tempfile.mkdtemp()
  try:
    temp_schedules_filepath = os.path.join(tempdir, 'schedules.csv')
    temp_cycletime_filepath = os.path.join(tempdir, 'cycletime.csv')
    _UpdateWaterfallUsageCsvs(temp_schedules_filepath,
                              temp_cycletime_filepath)
    real_schedules_path = _source_filepath(_BENCHMARK_SCHEDULES)
    if not filecmp.cmp(temp_schedules_filepath, real_schedules_path):
      errors.append(
          '{benchmark_schedules} is not up to date. Please run '
          '`./generate_perf_sharding.py update-timing --filter-only` '
          'to regenerate it.'.format(benchmark_schedules=_BENCHMARK_SCHEDULES))
    real_cycletime_path = _source_filepath(_CYCLETIME_CONTRIBUTION)
    if not filecmp.cmp(temp_cycletime_filepath, real_cycletime_path):
      errors.append(
          '{cycletimes} is not up to date. Please run '
          '`./generate_perf_sharding.py update-timing --filter-only` '
          'to regenerate it.'.format(cycletimes=_CYCLETIME_CONTRIBUTION))

    builders = _GetBuilderPlatforms(builders=None, waterfall='all')
    for builder in builders:
      output_file = os.path.join(
          tempdir, os.path.basename(builder.timing_file_path))
      _FilterTimingData(builder, output_file)
      if not filecmp.cmp(builder.timing_file_path, output_file):
        errors.append(
            '{timing_data} is not up to date. Please run '
            '`./generate_perf_sharding.py update-timing --filter-only` '
            'to regenerate it.'.format(timing_data=builder.timing_file_path))
  finally:
    shutil.rmtree(tempdir)

  # Check that bot_platforms.py matches the actual shard maps
  for platform in bot_platforms.ALL_PLATFORMS:
    platform_benchmark_names = set(
        b.name for b in platform.benchmark_configs) | set(
            e.name for e in platform.executables)
    shard_map_benchmark_names = _ParseBenchmarks(platform.shards_map_file_path)
    for benchmark in platform_benchmark_names - shard_map_benchmark_names:
      errors.append(
          'Benchmark {benchmark} is supposed to be scheduled on platform '
          '{platform} according to '
          'bot_platforms.py, but it is not yet scheduled. If this is a new '
          'benchmark, please rename it to UNSCHEDULED_{benchmark}, and then '
          'contact '
          'Telemetry and Chrome Client Infra team to schedule the benchmark. '
          'You can email chrome-benchmarking-request@ to get started.'.format(
              benchmark=benchmark, platform=platform.name))
    for benchmark in shard_map_benchmark_names - platform_benchmark_names:
      errors.append(
          'Benchmark {benchmark} is scheduled on shard map {path}, but '
          'bot_platforms.py '
          'says that it should not be on that shard map. This could be because '
          'the benchmark was deleted. If that is the case, you can use '
          '`generate_perf_sharding deschedule` to deschedule the benchmark '
          'from the shard map.'.format(
              benchmark=benchmark, path=platform.shards_map_file_path))

  # Check that every official benchmark is scheduled on some shard map.
  # TODO(crbug.com/963614): Note that this check can be deleted if we
  # find some way other than naming the benchmark with prefix "UNSCHEDULED_"
  # to make it clear that a benchmark is not running.
  scheduled_benchmarks = set()
  for platform in bot_platforms.ALL_PLATFORMS:
    scheduled_benchmarks = scheduled_benchmarks | _ParseBenchmarks(
        platform.shards_map_file_path)
  for benchmark in (
      bot_platforms.OFFICIAL_BENCHMARK_NAMES - scheduled_benchmarks):
    errors.append(
        'Benchmark {benchmark} is an official benchmark, but it is not '
        'scheduled to run anywhere. please rename it to '
        'UNSCHEDULED_{benchmark}'.format(benchmark=benchmark))

  for error in errors:
    print('*', textwrap.fill(error, 70), '\n', file=sys.stderr)
  if errors:
    return 1
  return 0


def main():
  parser = GetParser()
  options = parser.parse_args()
  return options.func(options)

if __name__ == '__main__':
  sys.exit(main())
