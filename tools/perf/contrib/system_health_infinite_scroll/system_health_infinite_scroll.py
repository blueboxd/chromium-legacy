# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import system_health
from telemetry import benchmark

from contrib.system_health_infinite_scroll import janky_story_set


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
    options.ExtendTraceCategoryFilter(['benchmark', 'cc', 'input'])
    options.SetTimelineBasedMetrics([
        'tbmv3:janky_time_per_scroll_processing_time',
        'tbmv3:num_excessive_touch_moves_blocking_gesture_scroll_updates',
    ])
    return options

  def CreateStorySet(self, options):
    return janky_story_set.JankyStorySet()
