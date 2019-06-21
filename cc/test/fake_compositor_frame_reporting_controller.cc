// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_compositor_frame_reporting_controller.h"

namespace cc {
FakeCompositorFrameReportingController::FakeCompositorFrameReportingController(
    bool is_single_threaded)
    : CompositorFrameReportingController(is_single_threaded) {}

void FakeCompositorFrameReportingController::WillBeginMainFrame() {
  if (!reporters_[PipelineStage::kBeginImplFrame])
    CompositorFrameReportingController::WillBeginImplFrame();
  CompositorFrameReportingController::WillBeginMainFrame();
}

void FakeCompositorFrameReportingController::BeginMainFrameAborted() {
  if (!reporters_[PipelineStage::kBeginMainFrame])
    WillBeginMainFrame();
  CompositorFrameReportingController::BeginMainFrameAborted();
}

void FakeCompositorFrameReportingController::WillCommit() {
  if (!reporters_[PipelineStage::kBeginMainFrame])
    WillBeginMainFrame();
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
}  // namespace cc
