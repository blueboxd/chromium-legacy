# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import system_health
import page_sets
from page_sets.system_health import story_tags
from telemetry import benchmark


@benchmark.Info(emails=['khokhlov@google.com'])
class SystemHealthInfiniteScroll(system_health.MobileCommonSystemHealth):
  """A subset of system_health.common_mobile benchmark.

  Contains only infinite_scroll stories.
  This benchmark is used for running experimental scroll jank metrics.

  TODO(b/150125501): Delete this benchmark once enough jank data have been
  obtained.
  """

  @classmethod
  def Name(cls):
    return 'system_health_infinite_scroll.common_mobile'

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = super(SystemHealthInfiniteScroll,
                    self).CreateCoreTimelineBasedMeasurementOptions()
    options.SetTimelineBasedMetrics(['tbmv3:scroll_jank_metric'])
    return options

  def CreateStorySet(self, options):
    return page_sets.SystemHealthStorySet(
        platform=self.PLATFORM, tag=story_tags.INFINITE_SCROLL)
