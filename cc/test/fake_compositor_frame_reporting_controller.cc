// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_compositor_frame_reporting_controller.h"

#include <utility>
#include <vector>

#include "components/viz/common/frame_timing_details.h"

namespace cc {
FakeCompositorFrameReportingController::FakeCompositorFrameReportingController(
    bool is_single_threaded)
    : CompositorFrameReportingController(is_single_threaded) {}

void FakeCompositorFrameReportingController::WillBeginMainFrame(
    const viz::BeginFrameId& id) {
  if (!reporters_[PipelineStage::kBeginImplFrame])
    CompositorFrameReportingController::WillBeginImplFrame(id);
  CompositorFrameReportingController::WillBeginMainFrame(id);
}

void FakeCompositorFrameReportingController::BeginMainFrameAborted(
    const viz::BeginFrameId& id) {
  if (!reporters_[PipelineStage::kBeginMainFrame])
    WillBeginMainFrame(id);
  CompositorFrameReportingController::BeginMainFrameAborted(id);
}

void FakeCompositorFrameReportingController::WillCommit() {
  if (!reporters_[PipelineStage::kBeginMainFrame])
    WillBeginMainFrame(viz::BeginFrameId());
  CompositorFrameReportingController::WillCommit();
}

void FakeCompositorFrameReportingController::DidCommit() {
  if (!reporters_[PipelineStage::kBeginMainFrame])
    WillCommit();
  CompositorFrameReportingController::DidCommit();
}

void FakeCompositorFrameReportingController::WillActivate() {
  if (!reporters_[PipelineStage::kCommit])
    DidCommit();
  CompositorFrameReportingController::WillActivate();
}

void FakeCompositorFrameReportingController::DidActivate() {
  if (!reporters_[PipelineStage::kCommit])
    WillActivate();
  CompositorFrameReportingController::DidActivate();
}

void FakeCompositorFrameReportingController::DidSubmitCompositorFrame(
    uint32_t frame_token,
    const viz::BeginFrameId& current_frame_id,
    const viz::BeginFrameId& last_activated_frame_id,
    std::vector<EventMetrics> events_metrics) {
  CompositorFrameReportingController::DidSubmitCompositorFrame(
      frame_token, current_frame_id, last_activated_frame_id,
      std::move(events_metrics));

  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = base::TimeTicks::Now();
  CompositorFrameReportingController::DidPresentCompositorFrame(frame_token,
                                                                details);
}

void FakeCompositorFrameReportingController::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {}
}  // namespace cc
