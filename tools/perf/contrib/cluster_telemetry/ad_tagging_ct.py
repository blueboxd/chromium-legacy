# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contrib.cluster_telemetry import ct_benchmarks_util, page_set
from core import perf_benchmark
import py_utils
from telemetry import benchmark
from telemetry.page import cache_temperature
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement

_NAVIGATION_TIMEOUT = 180
_QUIESCENCE_TIMEOUT = 30

# Benchmark to measure various UMA histograms relevant to AdTagging as well as
# CPU usage on page loads. These measurements will help to determine the
# accuracy of AdTagging.
@benchmark.Info(emails=['alexmt@chromium.org','johnidel@chromium.org'],
                component='UI>Browser>AdFilter')
class AdTaggingClusterTelemetry(perf_benchmark.PerfBenchmark):

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    ct_benchmarks_util.ValidateCommandLineArgs(parser, args)

  def GetExtraOutDirectories(self):
    # The indexed filter list does not end up in a build directory on cluster
    # telemetry; so we add its location here.
    return ['/b/s/w/ir/out/telemetry_isolates']

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.CreateLowOverheadFilter()
    tbm_options = timeline_based_measurement.Options(category_filter)
    uma_histograms = [
        'Ads.ResourceUsage.Size.Network.Mainframe.AdResource',
        'Ads.ResourceUsage.Size.Network.Mainframe.VanillaResource',
        'Ads.ResourceUsage.Size.Network.Subframe.AdResource',
        'Ads.ResourceUsage.Size.Network.Subframe.VanillaResource',
        'PageLoad.Clients.Ads.Cpu.AdFrames.Aggregate.TotalUsage',
        'PageLoad.Clients.Ads.Cpu.FullPage.TotalUsage',
        'PageLoad.Clients.Ads.Resources.Bytes.Ads2',
        'PageLoad.Experimental.Bytes.NetworkIncludingHeaders',
    ]
    for histogram in uma_histograms:
      tbm_options.config.chrome_trace_config.EnableUMAHistograms(histogram)

    tbm_options.SetTimelineBasedMetrics(['umaMetric', 'cpuTimeMetric'])
    return tbm_options

  def CreateStorySet(self, options):
    def NavigateToPageAndLeavePage(self, action_runner):
      url = self.file_path_url_with_scheme if self.is_file else self.url
      action_runner.Navigate(url,
                             self.script_to_evaluate_on_commit,
                             timeout_in_seconds=_NAVIGATION_TIMEOUT)
      try:
        py_utils.WaitFor(action_runner.tab.HasReachedQuiescence,
                         timeout=_QUIESCENCE_TIMEOUT)
      except py_utils.TimeoutException:
        pass

      # Navigate away to an untracked page to trigger recording of page load
      # metrics
      action_runner.Navigate('about:blank',
                             self.script_to_evaluate_on_commit,
                             timeout_in_seconds=_NAVIGATION_TIMEOUT)

    return page_set.CTPageSet(
        options.urls_list, options.user_agent, options.archive_data_file,
        run_navigate_steps_callback=NavigateToPageAndLeavePage,
        cache_temperature=cache_temperature.COLD)

  @classmethod
  def Name(cls):
    return 'ad_tagging.cluster_telemetry'
