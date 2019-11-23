// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/throughput_ukm_reporter.h"

#include "cc/trees/ukm_manager.h"

namespace cc {

namespace {
// Collect UKM once per kNumberOfSamplesToReport UMA reports.
constexpr unsigned kNumberOfSamplesToReport = 2000u;
}  // namespace

void ThroughputUkmReporter::ReportThroughputUkm(
    const UkmManager* ukm_manager,
    const base::Optional<int>& slower_throughput_percent,
    const base::Optional<int>& impl_throughput_percent,
    const base::Optional<int>& main_throughput_percent,
    FrameSequenceTrackerType type) {
  // Sampling control. We sample the event here to not throttle the UKM system.
  // Currently, the same sampling rate is applied to all existing trackers. We
  // might want to iterate on this based on the collected data.
  static uint32_t samples_to_next_event = 0;

  if (samples_to_next_event == 0) {
    // Sample every 2000 events. Using the Universal tracker as an example
    // which reports UMA every 5s, then the system collects UKM once per
    // 2000*5 = 10000 seconds, which is about 3 hours. This number may need to
    // be tuned to not throttle the UKM system.
    samples_to_next_event = kNumberOfSamplesToReport;
    if (impl_throughput_percent) {
      ukm_manager->RecordThroughputUKM(
          type, FrameSequenceTracker::ThreadType::kCompositor,
          impl_throughput_percent.value());
    }
    if (main_throughput_percent) {
      ukm_manager->RecordThroughputUKM(type,
                                       FrameSequenceTracker::ThreadType::kMain,
                                       main_throughput_percent.value());
    }
    ukm_manager->RecordThroughputUKM(type,
                                     FrameSequenceTracker::ThreadType::kSlower,
                                     slower_throughput_percent.value());
  }
  DCHECK_GT(samples_to_next_event, 0u);
  samples_to_next_event--;
}

}  // namespace cc
