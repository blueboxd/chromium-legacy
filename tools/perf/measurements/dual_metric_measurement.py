# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.web_perf import story_test
from telemetry.web_perf import timeline_based_measurement


class DualMetricMeasurement(story_test.StoryTest):
  """Test class for a benchmark that aggregates all metrics.

    Assumes both javascript as well as tracing metrics might be defined.

    All pages associated with this measurement must implement
    GetJavascriptMetricValues().
  """
  def __init__(self, tbm_options):
    super(DualMetricMeasurement, self).__init__()
    # Only enable tracing if metrics have been specified.
    if tbm_options.GetTimelineBasedMetrics():
      self._tbm_test = timeline_based_measurement.TimelineBasedMeasurement(
          tbm_options)
      self._enable_tracing = True
    else:
      self._enable_tracing = False

  def WillRunStory(self, platform):
    if self._enable_tracing:
      self._tbm_test.WillRunStory(platform)

  def Measure(self, platform, results):
    # There are four scenarios while we migrate the press benchmarks off of
    # the legacy value system
    # 1. Legacy Values that get converted with the call to PopulateHistogramSet
    #     Note: this only works when there is one page in the sotry
    # 2. Legacy Values with TMBv2 values.  Same note as #1
    # 3. Histograms added in the test.  Diagnostics must be added to these so
    #   all histograms must be added through AddHistogram call.
    # 4. Histograms added from the test as well as TBMv2 values.
    #   Diagnostics will get added by the timeline based measurement and the
    #   call to AddHistograms.
    if len(results.current_page.GetJavascriptMetricHistograms()) > 0:
      for histogram in results.current_page.GetJavascriptMetricHistograms():
        results.AddHistogram(histogram)
    else:
      for value in results.current_page.GetJavascriptMetricValues():
        results.AddValue(value)
      # This call is necessary to convert the current ScalarValues to
      # histograms before more histograms are added.  If we don't,
      # when histograms get added by TBM2 page_test_results will see those and
      # not convert any existing values because it assumes they are already
      # converted.  Therefore, so the javascript metrics don't get dropped, we
      # have to convert them first.
      # NOTE: this does not work if there is more than one page in this story.
      # It will drop results from all subsequent pages. See crbug.com/902812.
      results.PopulateHistogramSet()
    if self._enable_tracing:
      self._tbm_test.Measure(platform, results)

  def DidRunStory(self, platform, results):
    if self._enable_tracing:
      self._tbm_test.DidRunStory(platform, results)
