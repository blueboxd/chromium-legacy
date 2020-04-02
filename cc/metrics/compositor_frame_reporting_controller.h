// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
#define CC_METRICS_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/compositor_frame_reporter.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/frame_sequence_tracker.h"

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
struct BeginMainFrameMetrics;

// This is used for managing simultaneous CompositorFrameReporter instances
// in the case that the compositor has high latency. Calling one of the
// event functions will begin recording the time of the corresponding
// phase and trace it. If the frame is eventually submitted, then the
// recorded times of each phase will be reported in UMA.
// See CompositorFrameReporter.
class CC_EXPORT CompositorFrameReportingController {
 public:
  // Used as indices for accessing CompositorFrameReporters.
  enum PipelineStage {
    kBeginImplFrame = 0,
    kBeginMainFrame,
    kCommit,
    kActivate,
    kNumPipelineStages
  };

  explicit CompositorFrameReportingController(bool should_report_metrics);
  virtual ~CompositorFrameReportingController();

  CompositorFrameReportingController(
      const CompositorFrameReportingController&) = delete;
  CompositorFrameReportingController& operator=(
      const CompositorFrameReportingController&) = delete;

  // Events to signal Beginning/Ending of phases.
  virtual void WillBeginImplFrame(const viz::BeginFrameArgs& args);
  virtual void WillBeginMainFrame(const viz::BeginFrameArgs& args);
  virtual void BeginMainFrameAborted(const viz::BeginFrameId& id);
  virtual void WillInvalidateOnImplSide();
  virtual void WillCommit();
  virtual void DidCommit();
  virtual void WillActivate();
  virtual void DidActivate();
  virtual void DidSubmitCompositorFrame(
      uint32_t frame_token,
      const viz::BeginFrameId& current_frame_id,
      const viz::BeginFrameId& last_activated_frame_id,
      EventMetricsSet events_metrics);
  virtual void DidNotProduceFrame(const viz::BeginFrameId& id);
  virtual void OnFinishImplFrame(const viz::BeginFrameId& id);
  virtual void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details);

  void SetBlinkBreakdown(std::unique_ptr<BeginMainFrameMetrics> details,
                         base::TimeTicks main_thread_start_time);

  void SetUkmManager(UkmManager* manager);

  virtual void AddActiveTracker(FrameSequenceTrackerType type);
  virtual void RemoveActiveTracker(FrameSequenceTrackerType type);

  base::flat_set<FrameSequenceTrackerType> active_trackers_;

 protected:
  struct SubmittedCompositorFrame {
    uint32_t frame_token;
    std::unique_ptr<CompositorFrameReporter> reporter;
    SubmittedCompositorFrame();
    SubmittedCompositorFrame(uint32_t frame_token,
                             std::unique_ptr<CompositorFrameReporter> reporter);
    SubmittedCompositorFrame(SubmittedCompositorFrame&& other);
    ~SubmittedCompositorFrame();
  };
  base::TimeTicks Now() const;
  std::unique_ptr<CompositorFrameReporter>
      reporters_[PipelineStage::kNumPipelineStages];

 private:
  void AdvanceReporterStage(PipelineStage start, PipelineStage target);
  bool CanSubmitImplFrame(const viz::BeginFrameId& id) const;
  bool CanSubmitMainFrame(const viz::BeginFrameId& id) const;
  std::unique_ptr<CompositorFrameReporter> RestoreReporterAtBeginImpl(
      const viz::BeginFrameId& id);

  const bool should_report_metrics_;
  viz::BeginFrameId last_submitted_frame_id_;

  bool next_activate_has_invalidation_ = false;

  // The latency reporter passed to each CompositorFrameReporter. Owned here
  // because it must be common among all reporters.
  // DO NOT reorder this line and the ones below. The latency_ukm_reporter_ must
  // outlive the objects in |submitted_compositor_frames_|.
  std::unique_ptr<LatencyUkmReporter> latency_ukm_reporter_;

  // Mapping of frame token to pipeline reporter for submitted compositor
  // frames.
  // DO NOT reorder this line and the one above. The latency_ukm_reporter_ must
  // outlive the objects in |submitted_compositor_frames_|.
  base::circular_deque<SubmittedCompositorFrame> submitted_compositor_frames_;
};
}  // namespace cc

#endif  // CC_METRICS_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
