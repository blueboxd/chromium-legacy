// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/compositor_frame_reporter.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/rolling_time_delta_history.h"

namespace cc {
namespace {

// When considering if a time is abnormal, compare the stage execution
// time to this percentile from the previous times of the same stage.
static constexpr double kAbnormalityPercentile = 95;

// Use for determining abnormal execution times. If the sample size is less
// than this then don't check for abnormal execution time.
static constexpr size_t kMinimumTimeDeltaSampleSize = 20;

static constexpr int kMissedFrameReportTypeCount =
    static_cast<int>(CompositorFrameReporter::MissedFrameReportTypes::
                         kMissedFrameReportTypeCount);
static constexpr int kStageTypeCount =
    static_cast<int>(CompositorFrameReporter::StageType::kStageTypeCount);

// Names for CompositorFrameReporter::StageType, which should be updated in case
// of changes to the enum.
constexpr const char* kStageNames[]{
    "BeginImplFrameToSendBeginMainFrame",
    "SendBeginMainFrameToCommit",
    "Commit",
    "EndCommitToActivation",
    "Activation",
    "EndActivateToSubmitCompositorFrame",
    "SubmitCompositorFrameToPresentationCompositorFrame",
    "TotalLatency"};
static_assert(sizeof(kStageNames) / sizeof(kStageNames[0]) == kStageTypeCount,
              "Compositor latency stages has changed.");

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
static constexpr int kMaxHistogramIndex =
    2 * kMissedFrameReportTypeCount * kStageTypeCount;
static constexpr int kHistogramMin = 1;
static constexpr int kHistogramMax = 350000;
static constexpr int kHistogramBucketCount = 50;
}  // namespace

CompositorFrameReporter::CompositorFrameReporter(bool is_single_threaded)
    : is_single_threaded_(is_single_threaded) {
  TRACE_EVENT_ASYNC_BEGIN1("cc,benchmark", "PipelineReporter", this,
                           "is_single_threaded", is_single_threaded);
}

CompositorFrameReporter::~CompositorFrameReporter() {
  TerminateReporter();
}

void CompositorFrameReporter::StartStage(
    CompositorFrameReporter::StageType stage_type,
    base::TimeTicks start_time,
    RollingTimeDeltaHistory* stage_time_delta_history) {
  EndCurrentStage(start_time);
  current_stage_.stage_type = stage_type;
  current_stage_.start_time = start_time;
  current_stage_.time_delta_history = stage_time_delta_history;
  int stage_type_index = static_cast<int>(current_stage_.stage_type);
  CHECK_LT(stage_type_index, static_cast<int>(StageType::kStageTypeCount));
  CHECK_GE(stage_type_index, 0);
  TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(
      "cc,benchmark", "PipelineReporter", this,
      TRACE_STR_COPY(kStageNames[stage_type_index]), start_time);
}

void CompositorFrameReporter::EndCurrentStage(base::TimeTicks end_time) {
  if (current_stage_.start_time == base::TimeTicks())
    return;
  current_stage_.end_time = end_time;
  stage_history_.emplace_back(current_stage_);
  current_stage_.start_time = base::TimeTicks();
  current_stage_.time_delta_history = nullptr;
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

void CompositorFrameReporter::TerminateReporter() {
  DCHECK_EQ(current_stage_.start_time, base::TimeTicks());
  bool report_latency = false;
  const char* termination_status_str = nullptr;
  switch (frame_termination_status_) {
    case FrameTerminationStatus::kPresentedFrame:
      report_latency = true;
      termination_status_str = "presented_frame";
      break;
    case FrameTerminationStatus::kDidNotPresentFrame:
      termination_status_str = "did_not_present_frame";
      break;
    case FrameTerminationStatus::kMainFrameAborted:
      termination_status_str = "main_frame_aborted";
      break;
    case FrameTerminationStatus::kReplacedByNewReporter:
      termination_status_str = "replaced_by_new_reporter_at_same_stage";
      break;
    case FrameTerminationStatus::kDidNotProduceFrame:
      termination_status_str = "did_not_produce_frame";
      break;
    case FrameTerminationStatus::kUnknown:
      NOTREACHED();
      break;
  }

  const char* submission_status_str =
      submitted_frame_missed_deadline_ ? "missed_frame" : "non_missed_frame";

  TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP2(
      "cc,benchmark", "PipelineReporter", this, frame_termination_time_,
      "termination_status", TRACE_STR_COPY(termination_status_str),
      "compositor_frame_submission_status",
      TRACE_STR_COPY(submission_status_str));

  // Only report histograms if the frame was presented.
  if (report_latency) {
    DCHECK(stage_history_.size());
    stage_history_.emplace_back(
        StageData{StageType::kTotalLatency, stage_history_.front().start_time,
                  stage_history_.back().end_time, nullptr});
    ReportStageHistograms(submitted_frame_missed_deadline_);
  }
}

void CompositorFrameReporter::ReportStageHistograms(bool missed_frame) const {
  CompositorFrameReporter::MissedFrameReportTypes report_type =
      missed_frame
          ? CompositorFrameReporter::MissedFrameReportTypes::kMissedFrame
          : CompositorFrameReporter::MissedFrameReportTypes::kNonMissedFrame;

  for (const StageData& stage : stage_history_) {
    base::TimeDelta stage_delta = stage.end_time - stage.start_time;
    ReportHistogram(report_type, stage.stage_type, stage_delta);

    if (!stage.time_delta_history)
      continue;

    if (!missed_frame) {
      stage.time_delta_history->InsertSample(stage_delta);
    } else {
      // If enough sample data is recorded compare the stage duration with the
      // known normal stage duration and if it's higher than normal, report the
      // difference.
      if (stage.time_delta_history->sample_count() >=
          kMinimumTimeDeltaSampleSize) {
        base::TimeDelta time_upper_limit = GetStateNormalUpperLimit(stage);
        if (stage_delta > time_upper_limit) {
          ReportHistogram(CompositorFrameReporter::MissedFrameReportTypes::
                              kMissedFrameLatencyIncrease,
                          stage.stage_type, stage_delta - time_upper_limit);
        }
      }

      // In case of a missing frame, remove a sample from the recorded normal
      // stages. This invalidates the recorded normal durations if at a point
      // all frames start missing for a while.
      stage.time_delta_history->RemoveOldestSample();
    }
  }
}

void CompositorFrameReporter::ReportHistogram(
    CompositorFrameReporter::MissedFrameReportTypes report_type,
    CompositorFrameReporter::StageType stage_type,
    base::TimeDelta time_delta) const {
  const int report_type_index = static_cast<int>(report_type);
  const int stage_type_index = static_cast<int>(stage_type);
  const int histogram_index =
      (stage_type_index * kMissedFrameReportTypeCount + report_type_index) * 2 +
      (is_single_threaded_ ? 1 : 0);

  CHECK_LT(stage_type_index, kStageTypeCount);
  CHECK_GE(stage_type_index, 0);
  CHECK_LT(report_type_index, kMissedFrameReportTypeCount);
  CHECK_GE(report_type_index, 0);
  CHECK_LT(histogram_index, kMaxHistogramIndex);
  CHECK_GE(histogram_index, 0);

  const char* compositor_type = is_single_threaded_ ? "SingleThreaded" : "";
  const std::string name =
      base::StrCat({compositor_type, "CompositorLatency.",
                    kReportTypeNames[static_cast<int>(report_type)],
                    kStageNames[static_cast<int>(stage_type)]});

  STATIC_HISTOGRAM_POINTER_GROUP(
      name, histogram_index, kMaxHistogramIndex,
      AddTimeMicrosecondsGranularity(time_delta),
      base::Histogram::FactoryGet(
          name, kHistogramMin, kHistogramMax, kHistogramBucketCount,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}

base::TimeDelta CompositorFrameReporter::GetStateNormalUpperLimit(
    const StageData& stage) const {
  return stage.time_delta_history->Percentile(kAbnormalityPercentile);
}
}  // namespace cc
