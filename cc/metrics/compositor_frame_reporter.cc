// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporter.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/rolling_time_delta_history.h"
#include "cc/metrics/frame_sequence_tracker.h"

namespace cc {
namespace {

using StageType = CompositorFrameReporter::StageType;
using BlinkBreakdown = CompositorFrameReporter::BlinkBreakdown;
using VizBreakdown = CompositorFrameReporter::VizBreakdown;

constexpr int kMissedFrameReportTypeCount =
    static_cast<int>(CompositorFrameReporter::MissedFrameReportTypes::
                         kMissedFrameReportTypeCount);
constexpr int kStageTypeCount = static_cast<int>(StageType::kStageTypeCount);
constexpr int kAllBreakdownCount =
    static_cast<int>(VizBreakdown::kBreakdownCount) +
    static_cast<int>(BlinkBreakdown::kBreakdownCount);

constexpr int kVizBreakdownInitialIndex = kStageTypeCount;
constexpr int kBlinkBreakdownInitialIndex =
    kVizBreakdownInitialIndex + static_cast<int>(VizBreakdown::kBreakdownCount);

// For each possible FrameSequenceTrackerType there will be a UMA histogram
// plus one for general case.
constexpr int kFrameSequenceTrackerTypeCount =
    static_cast<int>(FrameSequenceTrackerType::kMaxType) + 1;

// Names for CompositorFrameReporter::StageType, which should be updated in case
// of changes to the enum.
constexpr const char* GetStageName(int stage_type_index) {
  switch (stage_type_index) {
    case static_cast<int>(StageType::kBeginImplFrameToSendBeginMainFrame):
      return "BeginImplFrameToSendBeginMainFrame";
    case static_cast<int>(StageType::kSendBeginMainFrameToCommit):
      return "SendBeginMainFrameToCommit";
    case static_cast<int>(StageType::kCommit):
      return "Commit";
    case static_cast<int>(StageType::kEndCommitToActivation):
      return "EndCommitToActivation";
    case static_cast<int>(StageType::kActivation):
      return "Activation";
    case static_cast<int>(StageType::kEndActivateToSubmitCompositorFrame):
      return "EndActivateToSubmitCompositorFrame";
    case static_cast<int>(
        StageType::kSubmitCompositorFrameToPresentationCompositorFrame):
      return "SubmitCompositorFrameToPresentationCompositorFrame";
    case static_cast<int>(StageType::kTotalLatency):
      return "TotalLatency";
    case static_cast<int>(VizBreakdown::kSubmitToReceiveCompositorFrame) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "SubmitToReceiveCompositorFrame";
    case static_cast<int>(VizBreakdown::kReceivedCompositorFrameToStartDraw) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "ReceivedCompositorFrameToStartDraw";
    case static_cast<int>(VizBreakdown::kStartDrawToSwapEnd) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "StartDrawToSwapEnd";
    case static_cast<int>(VizBreakdown::kSwapEndToPresentationCompositorFrame) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "SwapEndToPresentationCompositorFrame";
    case static_cast<int>(BlinkBreakdown::kHandleInputEvents) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.HandleInputEvents";
    case static_cast<int>(BlinkBreakdown::kAnimate) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.Animate";
    case static_cast<int>(BlinkBreakdown::kStyleUpdate) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.StyleUpdate";
    case static_cast<int>(BlinkBreakdown::kLayoutUpdate) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.LayoutUpdate";
    case static_cast<int>(BlinkBreakdown::kPrepaint) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.Prepaint";
    case static_cast<int>(BlinkBreakdown::kComposite) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.Composite";
    case static_cast<int>(BlinkBreakdown::kPaint) + kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.Paint";
    case static_cast<int>(BlinkBreakdown::kScrollingCoordinator) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.ScrollingCoordinator";
    case static_cast<int>(BlinkBreakdown::kCompositeCommit) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.CompositeCommit";
    case static_cast<int>(BlinkBreakdown::kUpdateLayers) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.UpdateLayers";
    default:
      return "";
  }
}

// Names for CompositorFrameReporter::MissedFrameReportTypes, which should be
// updated in case of changes to the enum.
constexpr const char* kReportTypeNames[]{"", "MissedFrame.",
                                         "MissedFrameLatencyIncrease."};

static_assert(sizeof(kReportTypeNames) / sizeof(kReportTypeNames[0]) ==
                  kMissedFrameReportTypeCount,
              "Compositor latency report types has changed.");

// This value should be recalculate in case of changes to the number of values
// in CompositorFrameReporter::MissedFrameReportTypes or in
// CompositorFrameReporter::StageType
constexpr int kMaxHistogramIndex = kMissedFrameReportTypeCount *
                                   kFrameSequenceTrackerTypeCount *
                                   (kStageTypeCount + kAllBreakdownCount);
constexpr int kHistogramMin = 1;
constexpr int kHistogramMax = 350000;
constexpr int kHistogramBucketCount = 50;

std::string HistogramName(const int report_type_index,
                          const int frame_sequence_tracker_type_index,
                          const int stage_type_index) {
  DCHECK_LE(frame_sequence_tracker_type_index,
            FrameSequenceTrackerType::kMaxType);
  const char* tracker_type_name =
      FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
          frame_sequence_tracker_type_index);
  DCHECK(tracker_type_name);
  return base::StrCat({"CompositorLatency.",
                       kReportTypeNames[report_type_index], tracker_type_name,
                       *tracker_type_name ? "." : "",
                       GetStageName(stage_type_index)});
}
}  // namespace

CompositorFrameReporter::CompositorFrameReporter(
    const base::flat_set<FrameSequenceTrackerType>* active_trackers,
    bool is_single_threaded)
    : is_single_threaded_(is_single_threaded),
      active_trackers_(active_trackers) {
  TRACE_EVENT_ASYNC_BEGIN1("cc,benchmark", "PipelineReporter", this,
                           "is_single_threaded", is_single_threaded);
}

CompositorFrameReporter::~CompositorFrameReporter() {
  TerminateReporter();
}

CompositorFrameReporter::StageData::StageData() = default;
CompositorFrameReporter::StageData::StageData(StageType stage_type,
                                              base::TimeTicks start_time,
                                              base::TimeTicks end_time)
    : stage_type(stage_type), start_time(start_time), end_time(end_time) {}
CompositorFrameReporter::StageData::StageData(const StageData&) = default;
CompositorFrameReporter::StageData::~StageData() = default;

void CompositorFrameReporter::StartStage(
    CompositorFrameReporter::StageType stage_type,
    base::TimeTicks start_time) {
  EndCurrentStage(start_time);
  current_stage_.stage_type = stage_type;
  current_stage_.start_time = start_time;
  int stage_type_index = static_cast<int>(current_stage_.stage_type);
  CHECK_LT(stage_type_index, static_cast<int>(StageType::kStageTypeCount));
  CHECK_GE(stage_type_index, 0);
  TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(
      "cc,benchmark", "PipelineReporter", this, GetStageName(stage_type_index),
      start_time);
}

void CompositorFrameReporter::EndCurrentStage(base::TimeTicks end_time) {
  if (current_stage_.start_time == base::TimeTicks())
    return;
  current_stage_.end_time = end_time;
  stage_history_.push_back(current_stage_);
  current_stage_.start_time = base::TimeTicks();
}

void CompositorFrameReporter::MissedSubmittedFrame() {
  submitted_frame_missed_deadline_ = true;
}

void CompositorFrameReporter::TerminateFrame(
    FrameTerminationStatus termination_status,
    base::TimeTicks termination_time) {
  frame_termination_status_ = termination_status;
  frame_termination_time_ = termination_time;
  EndCurrentStage(frame_termination_time_);
}

void CompositorFrameReporter::OnFinishImplFrame(base::TimeTicks timestamp) {
  DCHECK(!did_finish_impl_frame_);

  did_finish_impl_frame_ = true;
  impl_frame_finish_time_ = timestamp;
}

void CompositorFrameReporter::OnAbortBeginMainFrame() {
  did_abort_main_frame_ = false;
}

void CompositorFrameReporter::SetBlinkBreakdown(
    std::unique_ptr<BeginMainFrameMetrics> blink_breakdown) {
  DCHECK(blink_breakdown_.paint.is_zero());
  if (blink_breakdown)
    blink_breakdown_ = *blink_breakdown;
  else
    blink_breakdown_ = BeginMainFrameMetrics();
}

void CompositorFrameReporter::SetVizBreakdown(
    const viz::FrameTimingDetails& viz_breakdown) {
  DCHECK(viz_breakdown_.received_compositor_frame_timestamp.is_null());
  viz_breakdown_ = viz_breakdown;
}

void CompositorFrameReporter::TerminateReporter() {
  if (frame_termination_status_ != FrameTerminationStatus::kUnknown)
    DCHECK_EQ(current_stage_.start_time, base::TimeTicks());
  bool report_latency = false;
  const char* termination_status_str = nullptr;
  switch (frame_termination_status_) {
    case FrameTerminationStatus::kPresentedFrame:
      report_latency = true;
      termination_status_str = "presented_frame";
      break;
    case FrameTerminationStatus::kDidNotPresentFrame:
      report_latency = true;
      MissedSubmittedFrame();
      termination_status_str = "did_not_present_frame";
      break;
    case FrameTerminationStatus::kMainFrameAborted:
      termination_status_str = "main_frame_aborted";
      break;
    case FrameTerminationStatus::kReplacedByNewReporter:
      report_latency = true;
      MissedSubmittedFrame();
      termination_status_str = "replaced_by_new_reporter_at_same_stage";
      break;
    case FrameTerminationStatus::kDidNotProduceFrame:
      termination_status_str = "did_not_produce_frame";
      break;
    case FrameTerminationStatus::kUnknown:
      termination_status_str = "terminated_before_ending";
      break;
  }
  const char* submission_status_str =
      submitted_frame_missed_deadline_ ? "missed_frame" : "non_missed_frame";
  TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP2(
      "cc,benchmark", "PipelineReporter", this, frame_termination_time_,
      "termination_status", termination_status_str,
      "compositor_frame_submission_status", submission_status_str);

  // Only report histograms if the frame was presented.
  if (report_latency) {
    DCHECK(stage_history_.size());
    stage_history_.emplace_back(StageType::kTotalLatency,
                                stage_history_.front().start_time,
                                stage_history_.back().end_time);
    ReportStageHistograms(submitted_frame_missed_deadline_);
  }
}

void CompositorFrameReporter::ReportStageHistograms(bool missed_frame) const {
  CompositorFrameReporter::MissedFrameReportTypes report_type =
      missed_frame
          ? CompositorFrameReporter::MissedFrameReportTypes::kMissedFrame
          : CompositorFrameReporter::MissedFrameReportTypes::kNonMissedFrame;

  for (const StageData& stage : stage_history_) {
    ReportStageHistogramWithBreakdown(report_type, stage);

    for (const auto& frame_sequence_tracker_type : *active_trackers_) {
      // Report stage breakdowns.
      ReportStageHistogramWithBreakdown(report_type, stage,
                                        frame_sequence_tracker_type);
    }
  }
}

void CompositorFrameReporter::ReportStageHistogramWithBreakdown(
    CompositorFrameReporter::MissedFrameReportTypes report_type,
    const CompositorFrameReporter::StageData& stage,
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  base::TimeDelta stage_delta = stage.end_time - stage.start_time;
  ReportHistogram(report_type, frame_sequence_tracker_type,
                  static_cast<int>(stage.stage_type), stage_delta);
  switch (stage.stage_type) {
    case StageType::kSendBeginMainFrameToCommit: {
      ReportBlinkBreakdowns(report_type, frame_sequence_tracker_type);
      break;
    }
    case StageType::kSubmitCompositorFrameToPresentationCompositorFrame: {
      ReportVizBreakdowns(report_type, stage.start_time,
                          frame_sequence_tracker_type);
      break;
    }
    default:
      break;
  }
}

void CompositorFrameReporter::ReportBlinkBreakdowns(
    CompositorFrameReporter::MissedFrameReportTypes report_type,
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  std::vector<std::pair<BlinkBreakdown, base::TimeDelta>> breakdowns = {
      {BlinkBreakdown::kHandleInputEvents,
       blink_breakdown_.handle_input_events},
      {BlinkBreakdown::kAnimate, blink_breakdown_.animate},
      {BlinkBreakdown::kStyleUpdate, blink_breakdown_.style_update},
      {BlinkBreakdown::kLayoutUpdate, blink_breakdown_.layout_update},
      {BlinkBreakdown::kPrepaint, blink_breakdown_.prepaint},
      {BlinkBreakdown::kComposite, blink_breakdown_.composite},
      {BlinkBreakdown::kPaint, blink_breakdown_.paint},
      {BlinkBreakdown::kScrollingCoordinator,
       blink_breakdown_.scrolling_coordinator},
      {BlinkBreakdown::kCompositeCommit, blink_breakdown_.composite_commit},
      {BlinkBreakdown::kUpdateLayers, blink_breakdown_.update_layers}};

  for (const auto& pair : breakdowns) {
    ReportHistogram(report_type, frame_sequence_tracker_type,
                    kBlinkBreakdownInitialIndex + static_cast<int>(pair.first),
                    pair.second);
  }
}

void CompositorFrameReporter::ReportVizBreakdowns(
    CompositorFrameReporter::MissedFrameReportTypes report_type,
    base::TimeTicks start_time,
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  // Check if viz_breakdown is set.
  if (viz_breakdown_.received_compositor_frame_timestamp.is_null())
    return;
  base::TimeDelta submit_to_receive_compositor_frame_delta =
      viz_breakdown_.received_compositor_frame_timestamp - start_time;
  ReportHistogram(
      report_type, frame_sequence_tracker_type,
      kVizBreakdownInitialIndex +
          static_cast<int>(VizBreakdown::kSubmitToReceiveCompositorFrame),
      submit_to_receive_compositor_frame_delta);

  if (viz_breakdown_.draw_start_timestamp.is_null())
    return;
  base::TimeDelta received_compositor_frame_to_start_draw_delta =
      viz_breakdown_.draw_start_timestamp -
      viz_breakdown_.received_compositor_frame_timestamp;
  ReportHistogram(
      report_type, frame_sequence_tracker_type,
      kVizBreakdownInitialIndex +
          static_cast<int>(VizBreakdown::kReceivedCompositorFrameToStartDraw),
      received_compositor_frame_to_start_draw_delta);

  if (viz_breakdown_.swap_timings.is_null())
    return;
  base::TimeDelta start_draw_to_swap_end_delta =
      viz_breakdown_.swap_timings.swap_end -
      viz_breakdown_.draw_start_timestamp;

  ReportHistogram(report_type, frame_sequence_tracker_type,
                  kVizBreakdownInitialIndex +
                      static_cast<int>(VizBreakdown::kStartDrawToSwapEnd),
                  start_draw_to_swap_end_delta);

  base::TimeDelta swap_end_to_presentation_compositor_frame_delta =
      viz_breakdown_.presentation_feedback.timestamp -
      viz_breakdown_.swap_timings.swap_end;

  ReportHistogram(
      report_type, frame_sequence_tracker_type,
      kVizBreakdownInitialIndex +
          static_cast<int>(VizBreakdown::kSwapEndToPresentationCompositorFrame),
      swap_end_to_presentation_compositor_frame_delta);
}

void CompositorFrameReporter::ReportHistogram(
    CompositorFrameReporter::MissedFrameReportTypes report_type,
    FrameSequenceTrackerType frame_sequence_tracker_type,
    const int stage_type_index,
    base::TimeDelta time_delta) const {
  const int report_type_index = static_cast<int>(report_type);
  const int frame_sequence_tracker_type_index =
      static_cast<int>(frame_sequence_tracker_type);
  const int histogram_index =
      (stage_type_index * kFrameSequenceTrackerTypeCount +
       frame_sequence_tracker_type_index) *
          kMissedFrameReportTypeCount +
      report_type_index;

  CHECK_LT(stage_type_index, kStageTypeCount + kAllBreakdownCount);
  CHECK_GE(stage_type_index, 0);
  CHECK_LT(report_type_index, kMissedFrameReportTypeCount);
  CHECK_GE(report_type_index, 0);
  CHECK_LT(histogram_index, kMaxHistogramIndex);
  CHECK_GE(histogram_index, 0);

  STATIC_HISTOGRAM_POINTER_GROUP(
      HistogramName(report_type_index, frame_sequence_tracker_type_index,
                    stage_type_index),
      histogram_index, kMaxHistogramIndex,
      AddTimeMicrosecondsGranularity(time_delta),
      base::Histogram::FactoryGet(
          HistogramName(report_type_index, frame_sequence_tracker_type_index,
                        stage_type_index),
          kHistogramMin, kHistogramMax, kHistogramBucketCount,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}
}  // namespace cc
