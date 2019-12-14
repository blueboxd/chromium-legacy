// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/throughput_ukm_reporter.h"

#include "cc/trees/ukm_manager.h"

namespace cc {

namespace {
// Collect UKM once per kNumberOfSamplesToReport UMA reports.
constexpr unsigned kNumberOfSamplesToReport = 100u;
}  // namespace

void ThroughputUkmReporter::ReportThroughputUkm(
    const UkmManager* ukm_manager,
    const base::Optional<int>& slower_throughput_percent,
    const base::Optional<int>& impl_throughput_percent,
    const base::Optional<int>& main_throughput_percent,
    FrameSequenceTrackerType type) {
  if (samples_to_next_event_ == 0) {
    // Sample every 2000 events. Using the Universal tracker as an example
    // which reports UMA every 5s, then the system collects UKM once per
    // 2000*5 = 10000 seconds, which is about 3 hours. This number may need to
    // be tuned to not throttle the UKM system.
    samples_to_next_event_ = kNumberOfSamplesToReport;
    if (impl_throughput_percent) {
      ukm_manager->RecordThroughputUKM(
          type, FrameSequenceMetrics::ThreadType::kCompositor,
          impl_throughput_percent.value());
    }
    if (main_throughput_percent) {
      ukm_manager->RecordThroughputUKM(type,
                                       FrameSequenceMetrics::ThreadType::kMain,
                                       main_throughput_percent.value());
    }
    ukm_manager->RecordThroughputUKM(type,
                                     FrameSequenceMetrics::ThreadType::kSlower,
                                     slower_throughput_percent.value());
  }
  DCHECK_GT(samples_to_next_event_, 0u);
  samples_to_next_event_--;
}

}  // namespace cc
