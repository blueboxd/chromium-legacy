# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import urllib

from core import benchmark_finders


_SHARD_MAP_DIR = os.path.join(os.path.dirname(__file__), 'shard_maps')

_ALL_BENCHMARKS_BY_NAMES= dict(
    (b.Name(), b) for b in benchmark_finders.GetAllBenchmarks())

OFFICIAL_BENCHMARKS = frozenset(
    b for b in benchmark_finders.GetOfficialBenchmarks()
    if not b.Name().startswith('UNSCHEDULED_'))

def _IsPlatformSupported(benchmark, platform):
    supported = benchmark.GetSupportedPlatformNames(
        benchmark.SUPPORTED_PLATFORMS)
    return 'all' in supported or platform in supported


class PerfPlatform(object):
  def __init__(self, name, description, benchmark_names,
               num_shards, platform_os, is_fyi=False):
    self._name = name
    self._description = description
    self._platform_os = platform_os
    # For sorting ignore case and "segments" in the bot name.
    self._sort_key = name.lower().replace('-', ' ')
    self._is_fyi = is_fyi
    assert num_shards
    self._num_shards = num_shards
    benchmarks = []
    for benchmark_name in benchmark_names:
      benchmarks.append(_ALL_BENCHMARKS_BY_NAMES[benchmark_name])
    benchmarks_to_run = frozenset(benchmarks)
    # pylint: disable=redefined-outer-name
    self._benchmarks_to_run = frozenset([
        b for b in benchmarks_to_run if
          _IsPlatformSupported(b, self._platform_os)])
    # pylint: enable=redefined-outer-name

    base_file_name = name.replace(' ', '_').lower()
    self._timing_file_path = os.path.join(
        _SHARD_MAP_DIR, 'timing_data', base_file_name + '_timing.json')
    self._shards_map_file_path = os.path.join(
        _SHARD_MAP_DIR, base_file_name + '_map.json')

  def __lt__(self, other):
    if not isinstance(other, type(self)):
      return NotImplemented
    # pylint: disable=protected-access
    return self._sort_key < other._sort_key

  @property
  def num_shards(self):
    return self._num_shards

  @property
  def shards_map_file_path(self):
    return self._shards_map_file_path

  @property
  def timing_file_path(self):
    return self._timing_file_path

  @property
  def name(self):
    return self._name

  @property
  def description(self):
    return self._description

  @property
  def platform(self):
    return self._platform_os

  @property
  def benchmarks_to_run(self):
    return self._benchmarks_to_run

  @property
  def is_fyi(self):
    return self._is_fyi

  @property
  def buildbot_url(self):
    return ('https://ci.chromium.org/p/chrome/builders/luci.chrome.ci/%s' %
             urllib.quote(self._name))

OFFICIAL_BENCHMARK_NAMES = frozenset(
    b.Name() for b in OFFICIAL_BENCHMARKS)
_DL_BENCHMARK_NAMES = frozenset(['blink_perf.display_locking'])
_OFFICIAL_EXCEPT_DISPLAY_LOCKING = (
    OFFICIAL_BENCHMARK_NAMES - _DL_BENCHMARK_NAMES)

_LINUX_BENCHMARK_NAMES = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_MAC_HIGH_END_BENCHMARK_NAMES = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_MAC_LOW_END_BENCHMARK_NAMES = OFFICIAL_BENCHMARK_NAMES
_WIN_10_BENCHMARK_NAMES = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_WIN_10_LOW_END_HP_CANDIDATE_BENCHMARK_NAMES = frozenset([
    'v8.browsing_desktop'])
_WIN_7_BENCHMARK_NAMES = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_WIN_7_GPU_BENCHMARK_NAMES = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_ANDROID_GO_BENCHMARK_NAMES = frozenset([
    'system_health.memory_mobile',
    'system_health.common_mobile',
    'startup.mobile',
    'system_health.webview_startup',
    'v8.browsing_mobile',
    'speedometer',
    'speedometer2'])
_ANDROID_GO_WEBVIEW_BENCHMARK_NAMES = _ANDROID_GO_BENCHMARK_NAMES
_ANDROID_NEXUS_5_BENCHMARK_NAMES = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_ANDROID_NEXUS_5X_BENCHMARK_NAMES = OFFICIAL_BENCHMARK_NAMES
_ANDROID_NEXUS_5X_WEBVIEW_BENCHMARK_NAMES = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_ANDROID_NEXUS_6_WEBVIEW_BENCHMARK_NAMES = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_ANDROID_PIXEL2_BENCHMARK_NAMES = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_ANDROID_PIXEL2_WEBVIEW_BENCHMARK_NAMES = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_ANDROID_NEXUS5X_FYI_BENCHMARK_NAMES = frozenset([
    'heap_profiling.mobile.disabled',
    'heap_profiling.mobile.native',
    'heap_profiling.mobile.pseudo'])
_ANDROID_PIXEL2_FYI_BENCHMARK_NAMES = frozenset([
    'v8.browsing_mobile',
    'system_health.memory_mobile',
    'system_health.common_mobile',
    'startup.mobile',
    'speedometer2',
    'octane',
    'jetstream'])
_CHROMEOS_KEVIN_FYI_BENCHMARK_NAMES = frozenset([
    'rendering.desktop'])

# Linux
LINUX = PerfPlatform(
    'linux-perf', 'Ubuntu-14.04, 8 core, NVIDIA Quadro P400',
    _LINUX_BENCHMARK_NAMES, 26, 'linux')

# Mac
MAC_HIGH_END = PerfPlatform(
    'mac-10_13_laptop_high_end-perf',
    'MacBook Pro, Core i7 2.8 GHz, 16GB RAM, 256GB SSD, Radeon 55',
    _MAC_HIGH_END_BENCHMARK_NAMES, 26, 'mac')
MAC_LOW_END = PerfPlatform(
    'mac-10_12_laptop_low_end-perf',
    'MacBook Air, Core i5 1.8 GHz, 8GB RAM, 128GB SSD, HD Graphics',
    _MAC_LOW_END_BENCHMARK_NAMES, 26, 'mac')

# Win
WIN_10 = PerfPlatform(
    'win-10-perf',
    'Windows Intel HD 630 towers, Core i7-7700 3.6 GHz, 16GB RAM,'
    ' Intel Kaby Lake HD Graphics 630', _WIN_10_BENCHMARK_NAMES,
    26, 'win')
WIN_7 = PerfPlatform(
    'Win 7 Perf', 'N/A', _WIN_7_BENCHMARK_NAMES,
    5, 'win')
WIN_7_GPU = PerfPlatform(
    'Win 7 Nvidia GPU Perf', 'N/A', _WIN_7_GPU_BENCHMARK_NAMES,
    5, 'win')

# Android
ANDROID_GO = PerfPlatform(
    'android-go-perf', 'Android O (gobo)', _ANDROID_GO_BENCHMARK_NAMES,
    19, 'android')
ANDROID_GO_WEBVIEW = PerfPlatform(
    'android-go_webview-perf', 'Android OPM1.171019.021 (gobo)',
    _ANDROID_GO_WEBVIEW_BENCHMARK_NAMES, 13, 'android')
ANDROID_NEXUS_5 = PerfPlatform(
    'Android Nexus5 Perf', 'Android KOT49H', _ANDROID_NEXUS_5_BENCHMARK_NAMES,
    16, 'android')
ANDROID_NEXUS_5X = PerfPlatform(
    'android-nexus5x-perf', 'Android MMB29Q',
    _ANDROID_NEXUS_5X_BENCHMARK_NAMES, 16, 'android')
ANDROID_NEXUS_5X_WEBVIEW = PerfPlatform(
    'Android Nexus5X WebView Perf', 'Android AOSP MOB30K',
    _ANDROID_NEXUS_5X_WEBVIEW_BENCHMARK_NAMES, 16, 'android')
ANDROID_NEXUS_6_WEBVIEW = PerfPlatform(
    'Android Nexus6 WebView Perf', 'Android AOSP MOB30K',
    _ANDROID_NEXUS_6_WEBVIEW_BENCHMARK_NAMES,
    12,  # Reduced from 16 per crbug.com/891848.
    'android')
ANDROID_PIXEL2 = PerfPlatform(
    'android-pixel2-perf', 'Android OPM1.171019.021',
    _ANDROID_PIXEL2_BENCHMARK_NAMES, 35, 'android')
ANDROID_PIXEL2_WEBVIEW = PerfPlatform(
    'android-pixel2_webview-perf', 'Android OPM1.171019.021',
    _ANDROID_PIXEL2_WEBVIEW_BENCHMARK_NAMES, 28, 'android')

# FYI bots
WIN_10_LOW_END_HP_CANDIDATE = PerfPlatform(
    'win-10_laptop_low_end-perf_HP-Candidate', 'HP 15-BS121NR Laptop Candidate',
    _WIN_10_LOW_END_HP_CANDIDATE_BENCHMARK_NAMES,
    1, 'win', is_fyi=True)
ANDROID_NEXUS5X_PERF_FYI =  PerfPlatform(
    'android-nexus5x-perf-fyi', 'Android MMB29Q',
    _ANDROID_NEXUS5X_FYI_BENCHMARK_NAMES,
    3, 'android', is_fyi=True)
ANDROID_PIXEL2_PERF_FYI = PerfPlatform(
    'android-pixel2-perf-fyi', 'Android OPM1.171019.021',
    _ANDROID_PIXEL2_FYI_BENCHMARK_NAMES,
    4, 'android', is_fyi=True)
CHROMEOS_KEVIN_PERF_FYI = PerfPlatform(
    'chromeos-kevin-perf-fyi', '',
    _CHROMEOS_KEVIN_FYI_BENCHMARK_NAMES,
    4, 'chromeos', is_fyi=True)

# TODO(crbug.com/902089): Add linux-perf-fyi once the bot is configured to use
# the sharding map.

ALL_PLATFORMS = {
    p for p in locals().values() if isinstance(p, PerfPlatform)
}

FYI_PLATFORMS = {
    p for p in ALL_PLATFORMS if p.is_fyi
}

OFFICIAL_PLATFORMS = {
    p for p in ALL_PLATFORMS if not p.is_fyi
}

ALL_PLATFORM_NAMES = {
    p.name for p in ALL_PLATFORMS
}

OFFICIAL_PLATFORM_NAMES = {
    p.name for p in OFFICIAL_PLATFORMS
}
