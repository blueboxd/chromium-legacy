# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import memory
import page_sets

from telemetry import benchmark


# pylint: disable=protected-access
@benchmark.Info(emails=['etienneb@chromium.org'])
class LongRunningMemoryBenchmarkSitesDesktop(memory._MemoryInfra):
  """Measure memory usage on popular sites.

  This benchmark is intended to run locally over a long period of time. The
  data collected by this benchmark are not metrics but traces with memory dumps.
  The browser process is staying alive for the whole execution and memory dumps
  in these traces can be compare (diff) to determine which objects are potential
  memory leaks.
  """
  options = {
    'pageset_repeat': 30,
    'use_live_sites': True,
  }

  def CreateStorySet(self, options):
    return page_sets.DesktopMemoryPageSet()

  def SetExtraBrowserOptions(self, options):
    super(LongRunningMemoryBenchmarkSitesDesktop, self).SetExtraBrowserOptions(
        options)
    options.AppendExtraBrowserArgs([
        '--memlog=all', '--memlog-sampling',
        '--memlog-stack-mode=native-with-thread-names'])
    # Disable taking screenshot on failing pages.
    options.take_screenshot_for_failed_page = False

  @classmethod
  def Name(cls):
    return 'memory.long_running_desktop_sites'
