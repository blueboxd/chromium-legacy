// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/latency_ukm_reporter.h"

#include "base/rand_util.h"
#include "cc/trees/ukm_manager.h"

namespace cc {

void LatencyUkmReporter::ReportLatencyUkm(
    CompositorFrameReporter::FrameReportType report_type,
    const std::vector<CompositorFrameReporter::StageData>& stage_history,
    const base::flat_set<FrameSequenceTrackerType>* active_trackers,
    const viz::FrameTimingDetails& viz_breakdown) {
  if (!ukm_manager_)
    return;
  if (!frames_to_next_event_) {
    ukm_manager_->RecordLatencyUKM(report_type, stage_history, active_trackers,
                                   viz_breakdown);
    frames_to_next_event_ += SampleFramesToNextEvent();
  }
  DCHECK_GT(frames_to_next_event_, 0u);
  --frames_to_next_event_;
}

unsigned LatencyUkmReporter::SampleFramesToNextEvent() {
  // Return the test interval if set
  if (frames_to_next_event_for_test_)
    return frames_to_next_event_for_test_;

  // Sample from an exponential distribution to give a poisson distribution
  // of samples per time unit, then weigh it with an exponential multiplier to
  // give a few samples in rapid succession (for frames early in the page's
  // life) then exponentially fewer as the page lives longer.
  // RandDouble() returns [0,1), but we need (0,1]. If RandDouble() is
  // uniformly random, so is 1-RandDouble(), so use it to adjust the range.
  // When RandDouble returns 0.0, as it could, we will get a float_sample of
  // 0, causing underflow in UpdateEventTimeAndRecordIfNeeded. So rejection
  // sample until we get a positive count.
  double float_sample = 0;
  do {
    float_sample = -(sample_rate_multiplier_ *
                     std::exp(samples_so_far_ * sample_decay_rate_) *
                     std::log(1.0 - base::RandDouble()));
  } while (float_sample == 0);
  // float_sample is positive, so we don't need to worry about underflow.
  // After around 30 samples we will end up with a super high
  // sample. That's OK because it just means we'll stop reporting metrics
  // for that session, but we do need to be careful about overflow and NaN.
  samples_so_far_++;
  unsigned unsigned_sample =
      std::isnan(float_sample)
          ? UINT_MAX
          : base::saturated_cast<unsigned>(std::ceil(float_sample));
  return unsigned_sample;
}

}  // namespace cc
