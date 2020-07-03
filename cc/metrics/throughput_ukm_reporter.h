// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_THROUGHPUT_UKM_REPORTER_H_
#define CC_METRICS_THROUGHPUT_UKM_REPORTER_H_

#include "base/optional.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace cc {
class UkmManager;

enum class AggregationType {
  kAllAnimations,
  kAllInteractions,
  kAllSequences,
};

// A helper class that takes throughput data from a FrameSequenceTracker and
// talk to UkmManager to report it.
class CC_EXPORT ThroughputUkmReporter {
 public:
  explicit ThroughputUkmReporter(UkmManager* ukm_manager);
  ~ThroughputUkmReporter();

  ThroughputUkmReporter(const ThroughputUkmReporter&) = delete;
  ThroughputUkmReporter& operator=(const ThroughputUkmReporter&) = delete;

  void ReportThroughputUkm(const base::Optional<int>& slower_throughput_percent,
                           const base::Optional<int>& impl_throughput_percent,
                           const base::Optional<int>& main_throughput_percent,
                           FrameSequenceTrackerType type);

  void ReportAggregateThroughput(AggregationType aggregation_type,
                                 int throughput);

  void ComputeUniversalThroughput(FrameSequenceMetrics* metrics);

  // Once the kUniversal tracker reported its throughput to UMA, this returns
  // true. In this case, the |last_aggregated_percent_| and |last_impl_percent_|
  // must have value.
  bool HasThroughputData() const;
  // These functions are called only when HasThroughputData is true. They return
  // the throughput value of the corresponding thread, and reset them to nullopt
  // to indicate the value has been reported.
  int TakeLastAggregatedPercent();
  int TakeLastImplPercent();
  // This could be nullopt even if the HasThroughputData() is true, because it
  // could happen that all the frames are generated from the compositor thread.
  base::Optional<int> TakeLastMainPercent();

  uint32_t GetSamplesToNextEventForTesting(int index);

 private:
  // Sampling control. We sample the event here to not throttle the UKM system.
  // Currently, the same sampling rate is applied to all existing trackers. We
  // might want to iterate on this based on the collected data.
  uint32_t samples_to_next_event_[static_cast<int>(
      FrameSequenceTrackerType::kMaxType)] = {0};
  uint32_t samples_for_aggregated_report_ = 0;

  // This is pointing to the LayerTreeHostImpl::ukm_manager_, which is
  // initialized right after the LayerTreeHostImpl is created. So when this
  // pointer is initialized, there should be no trackers yet.
  UkmManager* const ukm_manager_;

  // The last "PercentDroppedFrames" reported to UMA. LayerTreeHostImpl will
  // read this number and send to the GPU process. When this page is done, we
  // will report to UKM using these numbers. Currently only meaningful to the
  // kUniversal tracker.
  // Possible values:
  //   1. A non-negative int value which is the percent of frames dropped.
  //   2. base::nullopt: when they are fetched by LayerTreeHostImpl, so that it
  //      knows that the last value has been reported.
  base::Optional<int> last_aggregated_percent_;
  base::Optional<int> last_main_percent_;
  base::Optional<int> last_impl_percent_;
};

}  // namespace cc

#endif  // CC_METRICS_THROUGHPUT_UKM_REPORTER_H_
