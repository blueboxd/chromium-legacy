// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/compositor_frame_reporting_controller.h"

#include "cc/scheduler/compositor_frame_reporter.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"

namespace cc {
namespace {
using StageType = CompositorFrameReporter::StageType;
RollingTimeDeltaHistory* GetStageHistory(
    std::unique_ptr<RollingTimeDeltaHistory> stage_history_[],
    StageType stage_type) {
  return stage_history_[static_cast<int>(stage_type)].get();
}

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
  for (auto& pair : submitted_compositor_frames_) {
    pair.reporter->TerminateFrame(
        CompositorFrameReporter::FrameTerminationStatus::kDidNotPresentFrame,
        Now());
  }
}

CompositorFrameReportingController::SubmittedCompositorFrame::
    SubmittedCompositorFrame() = default;
CompositorFrameReportingController::SubmittedCompositorFrame::
    SubmittedCompositorFrame(uint32_t frame_token,
                             std::unique_ptr<CompositorFrameReporter> reporter)
    : frame_token(frame_token), reporter(std::move(reporter)) {}
CompositorFrameReportingController::SubmittedCompositorFrame::
    ~SubmittedCompositorFrame() = default;

CompositorFrameReportingController::SubmittedCompositorFrame::
    SubmittedCompositorFrame(SubmittedCompositorFrame&& other) = default;

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
      GetStageHistory(stage_history_, CompositorFrameReporter::StageType::
                                          kBeginImplFrameToSendBeginMainFrame));
  reporters_[PipelineStage::kBeginImplFrame] = std::move(reporter);
}

void CompositorFrameReportingController::WillBeginMainFrame() {
  DCHECK(reporters_[PipelineStage::kBeginImplFrame]);
  // We need to use .get() below because operator<< in std::unique_ptr is a
  // C++20 feature.
  DCHECK_NE(reporters_[PipelineStage::kBeginMainFrame].get(),
            reporters_[PipelineStage::kBeginImplFrame].get());
  reporters_[PipelineStage::kBeginImplFrame]->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now(),
      GetStageHistory(
          stage_history_,
          CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit));
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
      GetStageHistory(stage_history_,
                      CompositorFrameReporter::StageType::kCommit));
}

void CompositorFrameReportingController::DidCommit() {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now(),
      GetStageHistory(
          stage_history_,
          CompositorFrameReporter::StageType::kEndCommitToActivation));
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
      GetStageHistory(stage_history_,
                      CompositorFrameReporter::StageType::kActivation));
}

void CompositorFrameReportingController::DidActivate() {
  DCHECK(reporters_[PipelineStage::kCommit] || next_activate_has_invalidation_);
  next_activate_has_invalidation_ = false;
  if (!reporters_[PipelineStage::kCommit])
    return;
  reporters_[PipelineStage::kCommit]->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now(),
      GetStageHistory(stage_history_, CompositorFrameReporter::StageType::
                                          kEndActivateToSubmitCompositorFrame));
  AdvanceReporterStage(PipelineStage::kCommit, PipelineStage::kActivate);
}

void CompositorFrameReportingController::DidSubmitCompositorFrame(
    uint32_t frame_token) {
  if (!reporters_[PipelineStage::kActivate])
    return;
  std::unique_ptr<CompositorFrameReporter> submitted_reporter =
      std::move(reporters_[PipelineStage::kActivate]);
  // If there are any other reporters active on the other stages of the
  // pipeline then that means a new frame was started during the duration of
  // this reporter and therefore the frame being tracked missed the deadline.
  if (reporters_[PipelineStage::kBeginImplFrame] ||
      reporters_[PipelineStage::kBeginMainFrame] ||
      reporters_[PipelineStage::kCommit]) {
    submitted_reporter->MissedSubmittedFrame();
  }
  submitted_reporter->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now(),
      GetStageHistory(stage_history_,
                      CompositorFrameReporter::StageType::
                          kSubmitCompositorFrameToPresentationCompositorFrame));
  submitted_compositor_frames_.emplace_back(frame_token,
                                            std::move(submitted_reporter));
}

void CompositorFrameReportingController::DidNotProduceFrame() {
  if (!reporters_[PipelineStage::kActivate])
    return;
  reporters_[PipelineStage::kActivate]->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotProduceFrame,
      Now());
  reporters_[PipelineStage::kActivate] = nullptr;
}

void CompositorFrameReportingController::DidPresentCompositorFrame(
    uint32_t frame_token,
    base::TimeTicks presentation_time) {
  while (!submitted_compositor_frames_.empty()) {
    auto submitted_frame = submitted_compositor_frames_.begin();
    if (viz::FrameTokenGT(submitted_frame->frame_token, frame_token))
      break;

    auto termination_status =
        CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame;
    if (submitted_frame->frame_token != frame_token)
      termination_status =
          CompositorFrameReporter::FrameTerminationStatus::kDidNotPresentFrame;

    submitted_frame->reporter->TerminateFrame(termination_status,
                                              presentation_time);
    submitted_compositor_frames_.erase(submitted_frame);
  }
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
