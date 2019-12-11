// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_THROUGHPUT_UKM_REPORTER_H_
#define CC_METRICS_THROUGHPUT_UKM_REPORTER_H_

#include "base/optional.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_sequence_tracker.h"

namespace cc {
class UkmManager;

// A helper class that takes throughput data from a FrameSequenceTracker and
// talk to UkmManager to report it.
class CC_EXPORT ThroughputUkmReporter {
 public:
  ThroughputUkmReporter() = default;
  ~ThroughputUkmReporter() = default;

  void ReportThroughputUkm(const UkmManager* ukm_manager,
                           const base::Optional<int>& slower_throughput_percent,
                           const base::Optional<int>& impl_throughput_percent,
                           const base::Optional<int>& main_throughput_percent,
                           FrameSequenceTrackerType type);

 private:
  // Sampling control. We sample the event here to not throttle the UKM system.
  // Currently, the same sampling rate is applied to all existing trackers. We
  // might want to iterate on this based on the collected data.
  uint32_t samples_to_next_event_ = 0;
};

}  // namespace cc

#endif  // CC_METRICS_THROUGHPUT_UKM_REPORTER_H_
