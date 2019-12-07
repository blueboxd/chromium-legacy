// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/throughput_ukm_reporter.h"

#include "cc/trees/ukm_manager.h"

namespace cc {

void ThroughputUkmReporter::ReportThroughputUkm(
    const UkmManager* ukm_manager,
    const base::Optional<int>& slower_throughput_percent,
    const base::Optional<int>& impl_throughput_percent,
    const base::Optional<int>& main_throughput_percent,
    FrameSequenceTrackerType type) {
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

}  // namespace cc
