// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/rolling_time_delta_history.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/metrics/latency_ukm_reporter.h"
#include "ui/events/types/event_type.h"

namespace cc {
namespace {

using StageType = CompositorFrameReporter::StageType;
using BlinkBreakdown = CompositorFrameReporter::BlinkBreakdown;
using VizBreakdown = CompositorFrameReporter::VizBreakdown;

constexpr int kFrameReportTypeCount =
    static_cast<int>(CompositorFrameReporter::FrameReportType::kMaxValue) + 1;
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

// Names for the viz breakdowns that are shown in trace as substages under
// PipelineReporter -> SubmitCompositorFrameToPresentationCompositorFrame
constexpr const char* GetVizBreakdownName(VizBreakdown stage) {
  switch (stage) {
    case VizBreakdown::kSubmitToReceiveCompositorFrame:
      return "SubmitToReceiveCompositorFrame";
    case VizBreakdown::kReceivedCompositorFrameToStartDraw:
      return "ReceiveCompositorFrameToStartDraw";
    case VizBreakdown::kStartDrawToSwapStart:
      return "StartDrawToSwapStart";
    case VizBreakdown::kSwapStartToSwapEnd:
      return "Swap";
    case VizBreakdown::kSwapEndToPresentationCompositorFrame:
      return "SwapEndToPresentationCompositorFrame";
    case VizBreakdown::kBreakdownCount:
      NOTREACHED();
      return "";
  }
}

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
    case static_cast<int>(VizBreakdown::kStartDrawToSwapStart) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "StartDrawToSwapStart";
    case static_cast<int>(VizBreakdown::kSwapStartToSwapEnd) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "SwapStartToSwapEnd";
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
    case static_cast<int>(BlinkBreakdown::kBeginMainSentToStarted) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.BeginMainSentToStarted";
    default:
      return "";
  }
}

// Names for CompositorFrameReporter::FrameReportType, which should be
// updated in case of changes to the enum.
constexpr const char* kReportTypeNames[]{"", "MissedDeadlineFrame.",
                                         "DroppedFrame."};

static_assert(base::size(kReportTypeNames) == kFrameReportTypeCount,
              "Compositor latency report types has changed.");

// This value should be recalculated in case of changes to the number of values
// in CompositorFrameReporter::DroppedFrameReportType or in
// CompositorFrameReporter::StageType
constexpr int kMaxCompositorLatencyHistogramIndex =
    kFrameReportTypeCount * kFrameSequenceTrackerTypeCount *
    (kStageTypeCount + kAllBreakdownCount);
constexpr int kCompositorLatencyHistogramMin = 1;
constexpr int kCompositorLatencyHistogramMax = 350000;
constexpr int kCompositorLatencyHistogramBucketCount = 50;

constexpr int kEventLatencyEventTypeCount =
    static_cast<int>(ui::EventType::ET_LAST);
constexpr int kEventLatencyScrollTypeCount =
    static_cast<int>(ScrollInputType::kMaxValue) + 1;
constexpr int kMaxEventLatencyHistogramIndex =
    kEventLatencyEventTypeCount * kEventLatencyScrollTypeCount;
constexpr int kEventLatencyHistogramMin = 1;
constexpr int kEventLatencyHistogramMax = 5000000;
constexpr int kEventLatencyHistogramBucketCount = 100;

bool ShouldReportLatencyMetricsForSequenceType(
    FrameSequenceTrackerType sequence_type) {
  return sequence_type != FrameSequenceTrackerType::kUniversal;
}

std::string GetCompositorLatencyHistogramName(
    const int report_type_index,
    FrameSequenceTrackerType frame_sequence_tracker_type,
    const int stage_type_index) {
  DCHECK_LE(frame_sequence_tracker_type, FrameSequenceTrackerType::kMaxType);
  DCHECK(
      ShouldReportLatencyMetricsForSequenceType(frame_sequence_tracker_type));
  const char* tracker_type_name =
      FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
          frame_sequence_tracker_type);
  DCHECK(tracker_type_name);
  return base::StrCat({"CompositorLatency.",
                       kReportTypeNames[report_type_index], tracker_type_name,
                       *tracker_type_name ? "." : "",
                       GetStageName(stage_type_index)});
}

std::string GetEventLatencyHistogramBaseName(
    const EventMetrics& event_metrics) {
  const bool is_scroll = event_metrics.scroll_input_type().has_value();
  return base::StrCat(
      {"EventLatency.", event_metrics.GetTypeName(), is_scroll ? "." : nullptr,
       is_scroll ? event_metrics.GetScrollTypeName() : nullptr});
}

}  // namespace

CompositorFrameReporter::CompositorFrameReporter(
    const base::flat_set<FrameSequenceTrackerType>* active_trackers,
    const viz::BeginFrameId& id,
    const base::TimeTicks frame_deadline,
    LatencyUkmReporter* latency_ukm_reporter)
    : frame_id_(id),
      active_trackers_(active_trackers),
      latency_ukm_reporter_(latency_ukm_reporter),
      frame_deadline_(frame_deadline) {}

std::unique_ptr<CompositorFrameReporter>
CompositorFrameReporter::CopyReporterAtBeginImplStage() const {
  if (stage_history_.empty() ||
      stage_history_.front().stage_type !=
          StageType::kBeginImplFrameToSendBeginMainFrame ||
      !did_finish_impl_frame())
    return nullptr;
  auto new_reporter = std::make_unique<CompositorFrameReporter>(
      active_trackers_, frame_id_, frame_deadline_, latency_ukm_reporter_);
  new_reporter->did_finish_impl_frame_ = did_finish_impl_frame_;
  new_reporter->impl_frame_finish_time_ = impl_frame_finish_time_;
  new_reporter->current_stage_.stage_type =
      StageType::kBeginImplFrameToSendBeginMainFrame;
  new_reporter->current_stage_.start_time = stage_history_.front().start_time;
  return new_reporter;
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
  if (frame_termination_status_ != FrameTerminationStatus::kUnknown)
    return;
  EndCurrentStage(start_time);
  current_stage_.stage_type = stage_type;
  current_stage_.start_time = start_time;
}

void CompositorFrameReporter::EndCurrentStage(base::TimeTicks end_time) {
  if (current_stage_.start_time == base::TimeTicks())
    return;
  current_stage_.end_time = end_time;
  stage_history_.push_back(current_stage_);
  current_stage_.start_time = base::TimeTicks();
}

void CompositorFrameReporter::TerminateFrame(
    FrameTerminationStatus termination_status,
    base::TimeTicks termination_time) {
  // If the reporter is already terminated, (possibly as a result of no damage)
  // then we don't need to do anything here, otherwise the reporter will be
  // terminated.
  if (frame_termination_status_ != FrameTerminationStatus::kUnknown)
    return;
  frame_termination_status_ = termination_status;
  frame_termination_time_ = termination_time;
  EndCurrentStage(frame_termination_time_);
}

void CompositorFrameReporter::OnFinishImplFrame(base::TimeTicks timestamp) {
  DCHECK(!did_finish_impl_frame_);

  did_finish_impl_frame_ = true;
  impl_frame_finish_time_ = timestamp;
}

void CompositorFrameReporter::OnAbortBeginMainFrame(base::TimeTicks timestamp) {
  DCHECK(!did_abort_main_frame_);

  did_abort_main_frame_ = true;
  impl_frame_finish_time_ = timestamp;
  // impl_frame_finish_time_ can be used for the end of BeginMain to Commit
  // stage
}

void CompositorFrameReporter::SetBlinkBreakdown(
    std::unique_ptr<BeginMainFrameMetrics> blink_breakdown,
    base::TimeTicks begin_main_start) {
  DCHECK(blink_breakdown_.paint.is_zero());
  if (blink_breakdown)
    blink_breakdown_ = *blink_breakdown;
  else
    blink_breakdown_ = BeginMainFrameMetrics();

  DCHECK(begin_main_frame_start_.is_null());
  begin_main_frame_start_ = begin_main_start;
}

void CompositorFrameReporter::SetVizBreakdown(
    const viz::FrameTimingDetails& viz_breakdown) {
  DCHECK(viz_breakdown_.received_compositor_frame_timestamp.is_null());
  viz_breakdown_ = viz_breakdown;
}

void CompositorFrameReporter::SetEventsMetrics(
    std::vector<EventMetrics> events_metrics) {
  DCHECK_EQ(0u, events_metrics_.size());
  events_metrics_ = std::move(events_metrics);
}

void CompositorFrameReporter::DroppedFrame() {
  report_type_ = FrameReportType::kDroppedFrame;
}

void CompositorFrameReporter::MissedDeadlineFrame() {
  report_type_ = FrameReportType::kMissedDeadlineFrame;
}

void CompositorFrameReporter::TerminateReporter() {
  if (frame_termination_status_ == FrameTerminationStatus::kUnknown)
    TerminateFrame(FrameTerminationStatus::kUnknown, base::TimeTicks::Now());
  DCHECK_EQ(current_stage_.start_time, base::TimeTicks());
  bool report_compositor_latency = false;
  bool report_event_latency = false;
  bool report_missed_deadline_frame = false;
  const char* termination_status_str = nullptr;
  switch (frame_termination_status_) {
    case FrameTerminationStatus::kPresentedFrame:
      report_compositor_latency = true;
      report_event_latency = true;
      termination_status_str = "presented_frame";
      if (frame_deadline_ < frame_termination_time_)
        report_missed_deadline_frame = true;
      break;
    case FrameTerminationStatus::kDidNotPresentFrame:
      report_compositor_latency = true;
      DroppedFrame();
      termination_status_str = "did_not_present_frame";
      break;
    case FrameTerminationStatus::kReplacedByNewReporter:
      report_compositor_latency = true;
      DroppedFrame();
      termination_status_str = "replaced_by_new_reporter_at_same_stage";
      break;
    case FrameTerminationStatus::kDidNotProduceFrame:
      termination_status_str = "did_not_produce_frame";
      break;
    case FrameTerminationStatus::kUnknown:
      termination_status_str = "terminated_before_ending";
      break;
  }

  ReportAllTraceEvents(termination_status_str);

  // Only report compositor latency histograms if the frame was produced.
  if (report_compositor_latency) {
    DCHECK(stage_history_.size());
    DCHECK_EQ(SumOfStageHistory(), stage_history_.back().end_time -
                                       stage_history_.front().start_time);
    stage_history_.emplace_back(StageType::kTotalLatency,
                                stage_history_.front().start_time,
                                stage_history_.back().end_time);
    ReportLatencyHistograms(report_event_latency, report_missed_deadline_frame);
  }
}

void CompositorFrameReporter::ReportLatencyHistograms(
    bool report_event_latency,
    bool report_delayed_latency) {
  ReportCompositorLatencyHistograms();

  if (report_delayed_latency) {
    // If the frames are delayed also report them under MissedDeadlineFrame.
    MissedDeadlineFrame();
    ReportCompositorLatencyHistograms();
  }

  // Only report event latency histograms if the frame was presented.
  if (report_event_latency)
    ReportEventLatencyHistograms();
}

void CompositorFrameReporter::ReportCompositorLatencyHistograms() const {
  UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type", report_type_);
  for (const StageData& stage : stage_history_) {
    ReportStageHistogramWithBreakdown(stage);

    for (const auto& frame_sequence_tracker_type : *active_trackers_) {
      // Report stage breakdowns.
      ReportStageHistogramWithBreakdown(stage, frame_sequence_tracker_type);
    }
  }
  if (latency_ukm_reporter_) {
    latency_ukm_reporter_->ReportLatencyUkm(report_type_, stage_history_,
                                            active_trackers_, viz_breakdown_);
  }
}

void CompositorFrameReporter::ReportStageHistogramWithBreakdown(
    const CompositorFrameReporter::StageData& stage,
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  if (!ShouldReportLatencyMetricsForSequenceType(frame_sequence_tracker_type))
    return;
  base::TimeDelta stage_delta = stage.end_time - stage.start_time;
  ReportCompositorLatencyHistogram(frame_sequence_tracker_type,
                                   static_cast<int>(stage.stage_type),
                                   stage_delta);
  switch (stage.stage_type) {
    case StageType::kSendBeginMainFrameToCommit:
      ReportBlinkBreakdowns(stage.start_time, frame_sequence_tracker_type);
      break;
    case StageType::kSubmitCompositorFrameToPresentationCompositorFrame:
      ReportVizBreakdowns(stage.start_time, frame_sequence_tracker_type);
      break;
    default:
      break;
  }
}

void CompositorFrameReporter::ReportBlinkBreakdowns(
    base::TimeTicks start_time,
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
      {BlinkBreakdown::kUpdateLayers, blink_breakdown_.update_layers},
      {BlinkBreakdown::kBeginMainSentToStarted,
       begin_main_frame_start_ - start_time}};

  for (const auto& pair : breakdowns) {
    ReportCompositorLatencyHistogram(
        frame_sequence_tracker_type,
        kBlinkBreakdownInitialIndex + static_cast<int>(pair.first),
        pair.second);
  }
}

void CompositorFrameReporter::ReportVizBreakdownStage(
    VizBreakdown stage,
    const base::TimeTicks start_time,
    const base::TimeTicks end_time,
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  base::TimeDelta time_delta = end_time - start_time;
  ReportCompositorLatencyHistogram(
      frame_sequence_tracker_type,
      kVizBreakdownInitialIndex + static_cast<int>(stage), time_delta);
}

void CompositorFrameReporter::ReportVizBreakdowns(
    base::TimeTicks start_time,
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  // Check if viz_breakdown is set. Testing indicates that sometimes the
  // received_compositor_frame_timestamp can be earlier than the given
  // start_time. Avoid reporting negative times.
  if (viz_breakdown_.received_compositor_frame_timestamp.is_null() ||
      viz_breakdown_.received_compositor_frame_timestamp < start_time) {
    return;
  }
  ReportVizBreakdownStage(VizBreakdown::kSubmitToReceiveCompositorFrame,
                          start_time,
                          viz_breakdown_.received_compositor_frame_timestamp,
                          frame_sequence_tracker_type);

  if (viz_breakdown_.draw_start_timestamp.is_null())
    return;
  ReportVizBreakdownStage(VizBreakdown::kReceivedCompositorFrameToStartDraw,
                          viz_breakdown_.received_compositor_frame_timestamp,
                          viz_breakdown_.draw_start_timestamp,
                          frame_sequence_tracker_type);

  if (viz_breakdown_.swap_timings.is_null())
    return;
  ReportVizBreakdownStage(
      VizBreakdown::kStartDrawToSwapStart, viz_breakdown_.draw_start_timestamp,
      viz_breakdown_.swap_timings.swap_start, frame_sequence_tracker_type);

  ReportVizBreakdownStage(
      VizBreakdown::kSwapStartToSwapEnd, viz_breakdown_.swap_timings.swap_start,
      viz_breakdown_.swap_timings.swap_end, frame_sequence_tracker_type);

  ReportVizBreakdownStage(VizBreakdown::kSwapEndToPresentationCompositorFrame,
                          viz_breakdown_.swap_timings.swap_end,
                          viz_breakdown_.presentation_feedback.timestamp,
                          frame_sequence_tracker_type);
}

void CompositorFrameReporter::ReportCompositorLatencyHistogram(
    FrameSequenceTrackerType frame_sequence_tracker_type,
    const int stage_type_index,
    base::TimeDelta time_delta) const {
  const int report_type_index = static_cast<int>(report_type_);
  const int frame_sequence_tracker_type_index =
      static_cast<int>(frame_sequence_tracker_type);
  const int histogram_index =
      (stage_type_index * kFrameSequenceTrackerTypeCount +
       frame_sequence_tracker_type_index) *
          kFrameReportTypeCount +
      report_type_index;

  CHECK_LT(stage_type_index, kStageTypeCount + kAllBreakdownCount);
  CHECK_GE(stage_type_index, 0);
  CHECK_LT(report_type_index, kFrameReportTypeCount);
  CHECK_GE(report_type_index, 0);
  CHECK_LT(histogram_index, kMaxCompositorLatencyHistogramIndex);
  CHECK_GE(histogram_index, 0);

  STATIC_HISTOGRAM_POINTER_GROUP(
      GetCompositorLatencyHistogramName(
          report_type_index, frame_sequence_tracker_type, stage_type_index),
      histogram_index, kMaxCompositorLatencyHistogramIndex,
      AddTimeMicrosecondsGranularity(time_delta),
      base::Histogram::FactoryGet(
          GetCompositorLatencyHistogramName(
              report_type_index, frame_sequence_tracker_type, stage_type_index),
          kCompositorLatencyHistogramMin, kCompositorLatencyHistogramMax,
          kCompositorLatencyHistogramBucketCount,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}

void CompositorFrameReporter::ReportEventLatencyHistograms() const {
  for (const EventMetrics& event_metrics : events_metrics_) {
    const std::string histogram_base_name =
        GetEventLatencyHistogramBaseName(event_metrics);
    const int event_type_index = static_cast<int>(event_metrics.type());
    const int scroll_type_index =
        event_metrics.scroll_input_type()
            ? static_cast<int>(*event_metrics.scroll_input_type())
            : 0;
    const int histogram_index =
        event_type_index * kEventLatencyScrollTypeCount + scroll_type_index;

    // For scroll events, report total latency up to gpu-swap-end. This is
    // useful in comparing new EventLatency metrics with LatencyInfo-based
    // scroll event latency metrics.
    if (event_metrics.scroll_input_type() &&
        !viz_breakdown_.swap_timings.is_null()) {
      const base::TimeDelta swap_end_latency =
          viz_breakdown_.swap_timings.swap_end - event_metrics.time_stamp();
      const std::string swap_end_histogram_name =
          histogram_base_name + ".TotalLatencyToSwapEnd";
      STATIC_HISTOGRAM_POINTER_GROUP(
          swap_end_histogram_name, histogram_index,
          kMaxEventLatencyHistogramIndex,
          AddTimeMicrosecondsGranularity(swap_end_latency),
          base::Histogram::FactoryGet(
              swap_end_histogram_name, kEventLatencyHistogramMin,
              kEventLatencyHistogramMax, kEventLatencyHistogramBucketCount,
              base::HistogramBase::kUmaTargetedHistogramFlag));
    }

    base::TimeDelta total_latency =
        frame_termination_time_ - event_metrics.time_stamp();
    const std::string histogram_name = histogram_base_name + ".TotalLatency";
    STATIC_HISTOGRAM_POINTER_GROUP(
        histogram_name, histogram_index, kMaxEventLatencyHistogramIndex,
        AddTimeMicrosecondsGranularity(total_latency),
        base::Histogram::FactoryGet(
            histogram_name, kEventLatencyHistogramMin,
            kEventLatencyHistogramMax, kEventLatencyHistogramBucketCount,
            base::HistogramBase::kUmaTargetedHistogramFlag));

    const auto trace_id = TRACE_ID_LOCAL(&event_metrics);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "cc,input", "EventLatency", trace_id, event_metrics.time_stamp(),
        "event", event_metrics.GetTypeName());

    // Report the breakdowns.
    // It is possible for an event to arrive in the compositor in the middle of
    // a frame (e.g. the browser received the event *after* renderer received a
    // begin-impl, and the event reached the compositor before that frame
    // ended). To handle such cases, find the first stage that happens after the
    // event's arrival in the browser.
    // TODO(mohsen): Report the breakdowns in UMA too.
    size_t index = 0;
    for (; index < stage_history_.size(); ++index) {
      const auto& stage = stage_history_[index];
      if (stage.start_time > event_metrics.time_stamp()) {
        TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
            "cc,input", "BrowserToRendererCompositor", trace_id,
            event_metrics.time_stamp());
        TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
            "cc,input", "BrowserToRendererCompositor", trace_id,
            stage.start_time);
        break;
      }
    }

    for (; index < stage_history_.size(); ++index) {
      const auto& stage = stage_history_[index];
      if (stage.stage_type == StageType::kTotalLatency)
        break;
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
          "cc,input", GetStageName(static_cast<int>(stage.stage_type)),
          trace_id, stage.start_time);
      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
          "cc,input", GetStageName(static_cast<int>(stage.stage_type)),
          trace_id, stage.end_time);
    }
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "cc,input", "EventLatency", trace_id, frame_termination_time_);
  }
}

void CompositorFrameReporter::ReportVizBreakdownTrace(
    VizBreakdown substage,
    const base::TimeTicks start_time,
    const base::TimeTicks end_time) const {
  // Do not report events with negative time span.
  if (end_time < start_time)
    return;

  const char* stage_name = GetVizBreakdownName(substage);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "cc,benchmark", stage_name, TRACE_ID_LOCAL(this), start_time);

  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "cc,benchmark", stage_name, TRACE_ID_LOCAL(this), end_time);
}

void CompositorFrameReporter::ReportAllTraceEvents(
    const char* termination_status_str) const {
  if (stage_history_.empty())
    return;

  const auto trace_id = TRACE_ID_LOCAL(this);
  const auto& startstage = stage_history_.front();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "cc,benchmark", "PipelineReporter", trace_id, startstage.start_time,
      "frame_id", frame_id_.ToString());

  // The trace-viewer cannot seem to handle a single child-event that has the
  // same start/end timestamps as the parent-event. So avoid adding the
  // child-events if there's only one.
  if (stage_history_.size() > 1) {
    for (const auto& stage : stage_history_) {
      const int stage_type_index = static_cast<int>(stage.stage_type);
      CHECK_LT(stage_type_index, static_cast<int>(StageType::kStageTypeCount));
      CHECK_GE(stage_type_index, 0);
      const char* name = GetStageName(stage_type_index);
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
          "cc,benchmark", name, trace_id, stage.start_time);

      if (stage.stage_type ==
          StageType::kSubmitCompositorFrameToPresentationCompositorFrame) {
        const struct {
          VizBreakdown stage;
          base::TimeTicks start_time;
          base::TimeTicks end_time;
        } sub_stages[] = {
            {VizBreakdown::kSubmitToReceiveCompositorFrame, stage.start_time,
             viz_breakdown_.received_compositor_frame_timestamp},
            {VizBreakdown::kReceivedCompositorFrameToStartDraw,
             viz_breakdown_.received_compositor_frame_timestamp,
             viz_breakdown_.draw_start_timestamp},
            {VizBreakdown::kStartDrawToSwapStart,
             viz_breakdown_.draw_start_timestamp,
             viz_breakdown_.swap_timings.swap_start},
            {VizBreakdown::kSwapStartToSwapEnd,
             viz_breakdown_.swap_timings.swap_start,
             viz_breakdown_.swap_timings.swap_end},
            {VizBreakdown::kSwapEndToPresentationCompositorFrame,
             viz_breakdown_.swap_timings.swap_end,
             viz_breakdown_.presentation_feedback.timestamp}};
        for (const auto& sub : sub_stages) {
          if (sub.start_time.is_null() || sub.end_time.is_null())
            break;
          ReportVizBreakdownTrace(sub.stage, sub.start_time, sub.end_time);
        }
      }

      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0("cc,benchmark", name,
                                                     trace_id, stage.end_time);
    }
  }

  const char* submission_status_str =
      report_type_ == FrameReportType::kDroppedFrame ? "dropped_frame"
                                                     : "non_dropped_frame";
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP2(
      "cc,benchmark", "PipelineReporter", trace_id,
      stage_history_.back().end_time, "termination_status",
      termination_status_str, "compositor_frame_submission_status",
      submission_status_str);
}

base::TimeDelta CompositorFrameReporter::SumOfStageHistory() const {
  base::TimeDelta sum;
  for (const StageData& stage : stage_history_)
    sum += stage.end_time - stage.start_time;
  return sum;
}

}  // namespace cc
