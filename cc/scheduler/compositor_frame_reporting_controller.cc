// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/compositor_frame_reporting_controller.h"

#include "cc/scheduler/compositor_frame_reporter.h"

namespace cc {
namespace {
static constexpr size_t kMaxHistorySize = 50;
}  // namespace

CompositorFrameReportingController::CompositorFrameReportingController(
    bool is_single_threaded)
    : is_single_threaded_(is_single_threaded) {
  for (int i = 0; i < static_cast<int>(
                          CompositorFrameReporter::StageType::kStageTypeCount);
       ++i) {
    stage_history_[i] =
        std::make_unique<RollingTimeDeltaHistory>(kMaxHistorySize);
  }
}

CompositorFrameReportingController::~CompositorFrameReportingController() {
  base::TimeTicks now = Now();
  for (int i = 0; i < PipelineStage::kNumPipelineStages; ++i) {
    if (reporters_[i]) {
      reporters_[i]->TerminateFrame(
          CompositorFrameReporter::FrameTerminationStatus::kDidNotProduceFrame,
          now);
    }
  }
}

base::TimeTicks CompositorFrameReportingController::Now() const {
  return base::TimeTicks::Now();
}

void CompositorFrameReportingController::WillBeginImplFrame() {
  base::TimeTicks begin_time = Now();
  if (reporters_[PipelineStage::kBeginImplFrame]) {
    reporters_[PipelineStage::kBeginImplFrame]->TerminateFrame(
        CompositorFrameReporter::FrameTerminationStatus::kReplacedByNewReporter,
        begin_time);
  }
  std::unique_ptr<CompositorFrameReporter> reporter =
      std::make_unique<CompositorFrameReporter>(is_single_threaded_);
  reporter->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      begin_time,
      stage_history_[static_cast<int>(CompositorFrameReporter::StageType::
                                          kBeginImplFrameToSendBeginMainFrame)]
          .get());
  reporters_[PipelineStage::kBeginImplFrame] = std::move(reporter);
}

void CompositorFrameReportingController::WillBeginMainFrame() {
  DCHECK(reporters_[PipelineStage::kBeginImplFrame]);
  DCHECK_NE(reporters_[PipelineStage::kBeginMainFrame],
            reporters_[PipelineStage::kBeginImplFrame]);
  reporters_[PipelineStage::kBeginImplFrame]->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now(),
      stage_history_[static_cast<int>(CompositorFrameReporter::StageType::
                                          kSendBeginMainFrameToCommit)]
          .get());
  AdvanceReporterStage(PipelineStage::kBeginImplFrame,
                       PipelineStage::kBeginMainFrame);
}

void CompositorFrameReportingController::BeginMainFrameAborted() {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  std::unique_ptr<CompositorFrameReporter> aborted_frame_reporter =
      std::move(reporters_[PipelineStage::kBeginMainFrame]);
  aborted_frame_reporter->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kMainFrameAborted,
      Now());
}

void CompositorFrameReportingController::WillCommit() {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->StartStage(
      CompositorFrameReporter::StageType::kCommit, Now(),
      stage_history_[static_cast<int>(
                         CompositorFrameReporter::StageType::kCommit)]
          .get());
}

void CompositorFrameReportingController::DidCommit() {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now(),
      stage_history_[static_cast<int>(CompositorFrameReporter::StageType::
                                          kEndCommitToActivation)]
          .get());
  AdvanceReporterStage(PipelineStage::kBeginMainFrame, PipelineStage::kCommit);
}

void CompositorFrameReportingController::WillInvalidateOnImplSide() {
  // Allows for activation without committing.
  // TODO(alsan): Report latency of impl side invalidations.
  next_activate_has_invalidation_ = true;
}

void CompositorFrameReportingController::WillActivate() {
  DCHECK(reporters_[PipelineStage::kCommit] || next_activate_has_invalidation_);
  if (!reporters_[PipelineStage::kCommit])
    return;
  reporters_[PipelineStage::kCommit]->StartStage(
      CompositorFrameReporter::StageType::kActivation, Now(),
      stage_history_[static_cast<int>(
                         CompositorFrameReporter::StageType::kActivation)]
          .get());
}

void CompositorFrameReportingController::DidActivate() {
  DCHECK(reporters_[PipelineStage::kCommit] || next_activate_has_invalidation_);
  next_activate_has_invalidation_ = false;
  if (!reporters_[PipelineStage::kCommit])
    return;
  reporters_[PipelineStage::kCommit]->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now(),
      stage_history_[static_cast<int>(CompositorFrameReporter::StageType::
                                          kEndActivateToSubmitCompositorFrame)]
          .get());
  AdvanceReporterStage(PipelineStage::kCommit, PipelineStage::kActivate);
}

void CompositorFrameReportingController::DidSubmitCompositorFrame() {
  if (!reporters_[PipelineStage::kActivate])
    return;
  base::TimeTicks submit_time = Now();
  std::unique_ptr<CompositorFrameReporter> submitted_reporter =
      std::move(reporters_[PipelineStage::kActivate]);
  // If there are any other reporters active on the other stages of the
  // pipeline then that means a new frame was started during the duration of
  // this reporter and therefore the frame being tracked missed the deadline.
  if (reporters_[PipelineStage::kBeginImplFrame] ||
      reporters_[PipelineStage::kBeginMainFrame] ||
      reporters_[PipelineStage::kCommit]) {
    submitted_reporter->TerminateFrame(
        CompositorFrameReporter::FrameTerminationStatus::
            kSubmittedFrameMissedDeadline,
        submit_time);
  } else {
    submitted_reporter->TerminateFrame(
        CompositorFrameReporter::FrameTerminationStatus::kSubmittedFrame,
        submit_time);
  }
}

void CompositorFrameReportingController::DidNotProduceFrame() {
  if (!reporters_[PipelineStage::kActivate])
    return;
  reporters_[PipelineStage::kActivate]->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotProduceFrame,
      Now());
  reporters_[PipelineStage::kActivate] = nullptr;
}

void CompositorFrameReportingController::AdvanceReporterStage(
    PipelineStage start,
    PipelineStage target) {
  if (reporters_[target]) {
    reporters_[target]->TerminateFrame(
        CompositorFrameReporter::FrameTerminationStatus::kReplacedByNewReporter,
        Now());
  }
  reporters_[target] = std::move(reporters_[start]);
}
}  // namespace cc
