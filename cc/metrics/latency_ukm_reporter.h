// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_LATENCY_UKM_REPORTER_H_
#define CC_METRICS_LATENCY_UKM_REPORTER_H_

#include "base/optional.h"
#include "cc/cc_export.h"
#include "cc/metrics/compositor_frame_reporter.h"
#include "components/viz/common/frame_timing_details.h"

namespace cc {
class UkmManager;

// A helper class that takes latency data from a CompositorFrameReporter and
// talks to UkmManager to report it.
class CC_EXPORT LatencyUkmReporter {
 public:
  LatencyUkmReporter() = default;
  ~LatencyUkmReporter() = default;

  void ReportLatencyUkm(
      CompositorFrameReporter::FrameReportType report_type,
      const std::vector<CompositorFrameReporter::StageData>& stage_history,
      const base::flat_set<FrameSequenceTrackerType>* active_trackers,
      const viz::FrameTimingDetails& viz_breakdown);

  void SetUkmManager(UkmManager* manager) { ukm_manager_ = manager; }

 private:
  unsigned SampleFramesToNextEvent();

  // To test event sampling. This and all future intervals will be the given
  // frame count, until this is called again.
  void FramesToNextEventForTest(unsigned num_frames) {
    frames_to_next_event_for_test_ = num_frames;
  }

  // This is pointing to the LayerTreeHostImpl::ukm_manager_, which is
  // initialized right after the LayerTreeHostImpl is created. So when this
  // pointer is initialized, there should be no trackers yet. Moreover, the
  // LayerTreeHostImpl::ukm_manager_ lives as long as the LayerTreeHostImpl, so
  // this pointer should never be null as long as LayerTreeHostImpl is alive.
  UkmManager* ukm_manager_ = nullptr;

  // Sampling control. We use a Poisson process with an exponential decay
  // multiplier. The goal is to get many randomly distributed samples early
  // during page load and initial interaction, then samples at an exponentially
  // decreasing rate to effectively cap the number of samples. The particular
  // parameters chosen here give roughly 5-10 samples in the first 100 frames,
  // decaying to several hours between samples by the 40th sample. The
  // multiplier value should be tuned to achieve a total sample count that
  // avoids throttling by the UKM system.
  // The sample_rate_multiplier_ and sample_decay_rate_ have been set to meet
  // UKM goals for data volume.
  double sample_decay_rate_ = 1;
  double sample_rate_multiplier_ = 2;
  unsigned samples_so_far_ = 0;
  unsigned frames_to_next_event_ = 0;

  // Test data, used for SampleFramesToNextEvent if present
  unsigned frames_to_next_event_for_test_ = 0;
};

}  // namespace cc

#endif  // CC_METRICS_LATENCY_UKM_REPORTER_H_
