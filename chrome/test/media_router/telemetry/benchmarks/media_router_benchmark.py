# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from core import perf_benchmark
from core import path_util

from benchmarks.pagesets import media_router_perf_pages
from benchmarks import media_router_measurements


class _BaseCastBenchmark(perf_benchmark.PerfBenchmark):
  options = {'pageset_repeat': 6}

  page_set = media_router_perf_pages.MediaRouterDialogPageSet

  def SetExtraBrowserOptions(self, options):
    options.flush_os_page_caches_on_start = True
    # This flag is required to enable the communication between the page and
    # the test extension.
    options.disable_background_networking = False

    # TODO: find a better way to find extension location.
    options.AppendExtraBrowserArgs([
        '--load-extension=' + ','.join([
            os.path.join(path_util.GetChromiumSrcDir(), 'out',
            'Release', 'mr_extension', 'release'),
             os.path.join(path_util.GetChromiumSrcDir(), 'out',
             'Release', 'media_router', 'telemetry_extension')]),
        '--disable-features=ViewsCastDialog',
        '--whitelisted-extension-id=enhhojjnijigcajfphajepfemndkmdlo',
        '--media-router=1',
        '--enable-stats-collection-bindings'
    ])


class CPUMemoryCastBenckmark(_BaseCastBenchmark):
  """Benchmark for CPU and memory usage with Media Router."""

  options = {'pageset_repeat': 1}

  page_set = media_router_perf_pages.MediaRouterCPUMemoryPageSet

  def CreatePageTest(self, options):
    return media_router_measurements.MediaRouterCPUMemoryTest()

  @classmethod
  def Name(cls):
    return 'media_router.cpu_memory'


class CPUMemoryBenckmark(perf_benchmark.PerfBenchmark):
  """Benchmark for CPU and memory usage without Media Router."""

  options = {'pageset_repeat': 1}

  page_set = media_router_perf_pages.CPUMemoryPageSet

  def SetExtraBrowserOptions(self, options):
    options.flush_os_page_caches_on_start = True
    # This flag is required to enable the communication between the page and
    # the test extension.
    options.disable_background_networking = False
    options.AppendExtraBrowserArgs([
        '--load-extension=' +
             os.path.join(path_util.GetChromiumSrcDir(), 'out',
             'Release', 'media_router', 'telemetry_extension'),
        '--disable-features=ViewsCastDialog',
        '--media-router=0',
        '--enable-stats-collection-bindings'
    ])

  def CreatePageTest(self, options):
    return media_router_measurements.MediaRouterCPUMemoryTest()

  @classmethod
  def Name(cls):
    return 'media_router.cpu_memory.no_media_router'
