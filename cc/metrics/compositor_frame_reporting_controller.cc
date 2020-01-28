// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporting_controller.h"

#include "cc/metrics/compositor_frame_reporter.h"
#include "cc/metrics/latency_ukm_reporter.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"

namespace cc {
namespace {
using StageType = CompositorFrameReporter::StageType;
}  // namespace

CompositorFrameReportingController::CompositorFrameReportingController(
    bool is_single_threaded)
    : is_single_threaded_(is_single_threaded),
      latency_ukm_reporter_(std::make_unique<LatencyUkmReporter>()) {}

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

void CompositorFrameReportingController::WillBeginImplFrame(
    const viz::BeginFrameId& id) {
  base::TimeTicks begin_time = Now();
  if (reporters_[PipelineStage::kBeginImplFrame]) {
    // If the the reporter is replaced in this stage, it means that Impl frame
    // caused no damage.
    reporters_[PipelineStage::kBeginImplFrame]->TerminateFrame(
        CompositorFrameReporter::FrameTerminationStatus::kDidNotProduceFrame,
        begin_time);
  }
  std::unique_ptr<CompositorFrameReporter> reporter =
      std::make_unique<CompositorFrameReporter>(&active_trackers_, id,
                                                latency_ukm_reporter_.get(),
                                                is_single_threaded_);
  reporter->StartStage(StageType::kBeginImplFrameToSendBeginMainFrame,
                       begin_time);
  reporters_[PipelineStage::kBeginImplFrame] = std::move(reporter);
}

void CompositorFrameReportingController::WillBeginMainFrame(
    const viz::BeginFrameId& id) {
  if (reporters_[PipelineStage::kBeginImplFrame]) {
    // We need to use .get() below because operator<< in std::unique_ptr is a
    // C++20 feature.
    DCHECK_NE(reporters_[PipelineStage::kBeginMainFrame].get(),
              reporters_[PipelineStage::kBeginImplFrame].get());
    DCHECK_EQ(reporters_[PipelineStage::kBeginImplFrame]->frame_id_, id);
    reporters_[PipelineStage::kBeginImplFrame]->StartStage(
        StageType::kSendBeginMainFrameToCommit, Now());
    AdvanceReporterStage(PipelineStage::kBeginImplFrame,
                         PipelineStage::kBeginMainFrame);
  } else {
    // In this case we have already submitted the ImplFrame, but we received
    // beginMain frame before next BeginImplFrame (Not reached the ImplFrame
    // deadline yet). So will start a new reporter at BeginMainFrame.
    std::unique_ptr<CompositorFrameReporter> reporter =
        std::make_unique<CompositorFrameReporter>(&active_trackers_, id,
                                                  latency_ukm_reporter_.get(),
                                                  is_single_threaded_);
    reporter->StartStage(StageType::kSendBeginMainFrameToCommit, Now());
    reporters_[PipelineStage::kBeginMainFrame] = std::move(reporter);
  }
}

void CompositorFrameReportingController::BeginMainFrameAborted(
    const viz::BeginFrameId& id) {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  DCHECK_EQ(reporters_[PipelineStage::kBeginMainFrame]->frame_id_, id);
  auto& begin_main_reporter = reporters_[PipelineStage::kBeginMainFrame];
  begin_main_reporter->OnAbortBeginMainFrame();
}

void CompositorFrameReportingController::WillCommit() {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->StartStage(StageType::kCommit,
                                                         Now());
}

void CompositorFrameReportingController::DidCommit() {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->StartStage(
      StageType::kEndCommitToActivation, Now());
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
  reporters_[PipelineStage::kCommit]->StartStage(StageType::kActivation, Now());
}

void CompositorFrameReportingController::DidActivate() {
  DCHECK(reporters_[PipelineStage::kCommit] || next_activate_has_invalidation_);
  next_activate_has_invalidation_ = false;
  if (!reporters_[PipelineStage::kCommit])
    return;
  reporters_[PipelineStage::kCommit]->StartStage(
      StageType::kEndActivateToSubmitCompositorFrame, Now());
  AdvanceReporterStage(PipelineStage::kCommit, PipelineStage::kActivate);
}

void CompositorFrameReportingController::DidSubmitCompositorFrame(
    uint32_t frame_token,
    const viz::BeginFrameId& current_frame_id,
    const viz::BeginFrameId& last_activated_frame_id) {
  // If the last_activated_frame_id from scheduler is the same as
  // last_submitted_frame_id_ in reporting controller, this means that we are
  // submitting the Impl frame. In this case the frame will be submitted if
  // Impl work is finished.
  bool is_activated_frame_new =
      (last_activated_frame_id != last_submitted_frame_id_);
  if (is_activated_frame_new) {
    DCHECK_EQ(reporters_[PipelineStage::kActivate]->frame_id_,
              last_activated_frame_id);
    // The reporter in activate state can be submitted
  } else {
    // There is no Main damage, which is possible if (1) there was no beginMain
    // so the reporter in beginImpl will be submitted or (2) the beginMain is
    // sent and aborted, so the reporter in beginMain will be submitted.
    if (CanSubmitImplFrame(current_frame_id)) {
      auto& reporter = reporters_[PipelineStage::kBeginImplFrame];
      reporter->StartStage(StageType::kEndActivateToSubmitCompositorFrame,
                           reporter->impl_frame_finish_time());
      AdvanceReporterStage(PipelineStage::kBeginImplFrame,
                           PipelineStage::kActivate);
    } else if (CanSubmitMainFrame(current_frame_id)) {
      auto& reporter = reporters_[PipelineStage::kBeginMainFrame];
      reporter->StartStage(StageType::kEndActivateToSubmitCompositorFrame,
                           reporter->impl_frame_finish_time());
      AdvanceReporterStage(PipelineStage::kBeginMainFrame,
                           PipelineStage::kActivate);
    } else {
      return;
    }
  }

  last_submitted_frame_id_ = last_activated_frame_id;
  std::unique_ptr<CompositorFrameReporter> submitted_reporter =
      std::move(reporters_[PipelineStage::kActivate]);
  submitted_reporter->StartStage(
      StageType::kSubmitCompositorFrameToPresentationCompositorFrame, Now());
  submitted_compositor_frames_.emplace_back(frame_token,
                                            std::move(submitted_reporter));
}

void CompositorFrameReportingController::DidNotProduceFrame(
    const viz::BeginFrameId& id) {
  for (auto& stage_reporter : reporters_) {
    if (stage_reporter && stage_reporter->frame_id_ == id) {
      stage_reporter->TerminateFrame(
          CompositorFrameReporter::FrameTerminationStatus::kDidNotProduceFrame,
          Now());
      return;
    }
  }
}

void CompositorFrameReportingController::OnFinishImplFrame(
    const viz::BeginFrameId& id) {
  if (reporters_[PipelineStage::kBeginImplFrame]) {
    DCHECK_EQ(reporters_[PipelineStage::kBeginImplFrame]->frame_id_, id);
    reporters_[PipelineStage::kBeginImplFrame]->OnFinishImplFrame(Now());
  } else if (reporters_[PipelineStage::kBeginMainFrame]) {
    DCHECK_EQ(reporters_[PipelineStage::kBeginMainFrame]->frame_id_, id);
    auto& begin_main_reporter = reporters_[PipelineStage::kBeginMainFrame];
    begin_main_reporter->OnFinishImplFrame(Now());
  }
}

void CompositorFrameReportingController::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  while (!submitted_compositor_frames_.empty()) {
    auto submitted_frame = submitted_compositor_frames_.begin();
    if (viz::FrameTokenGT(submitted_frame->frame_token, frame_token))
      break;

    auto termination_status =
        CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame;
    if (submitted_frame->frame_token != frame_token)
      termination_status =
          CompositorFrameReporter::FrameTerminationStatus::kDidNotPresentFrame;

    submitted_frame->reporter->SetVizBreakdown(details);
    submitted_frame->reporter->TerminateFrame(
        termination_status, details.presentation_feedback.timestamp);
    submitted_compositor_frames_.erase(submitted_frame);
  }
}

void CompositorFrameReportingController::SetBlinkBreakdown(
    std::unique_ptr<BeginMainFrameMetrics> details,
    base::TimeTicks main_thread_start_time) {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->SetBlinkBreakdown(
      std::move(details), main_thread_start_time);
}

void CompositorFrameReportingController::AddActiveTracker(
    FrameSequenceTrackerType type) {
  active_trackers_.insert(type);
}

void CompositorFrameReportingController::RemoveActiveTracker(
    FrameSequenceTrackerType type) {
  active_trackers_.erase(type);
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

bool CompositorFrameReportingController::CanSubmitImplFrame(
    const viz::BeginFrameId& id) const {
  if (!reporters_[PipelineStage::kBeginImplFrame])
    return false;
  auto& reporter = reporters_[PipelineStage::kBeginImplFrame];
  return (reporter->frame_id_ == id && reporter->did_finish_impl_frame());
}

bool CompositorFrameReportingController::CanSubmitMainFrame(
    const viz::BeginFrameId& id) const {
  if (!reporters_[PipelineStage::kBeginMainFrame])
    return false;
  auto& reporter = reporters_[PipelineStage::kBeginMainFrame];
  return (reporter->frame_id_ == id && reporter->did_finish_impl_frame() &&
          reporter->did_abort_main_frame());
}

void CompositorFrameReportingController::SetUkmManager(UkmManager* manager) {
  latency_ukm_reporter_->SetUkmManager(manager);
}

}  // namespace cc
