// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_tracker.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/throughput_ukm_reporter.h"
#include "cc/trees/ukm_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "ui/gfx/presentation_feedback.h"

// This macro is used with DCHECK to provide addition debug info.
#if DCHECK_IS_ON()
#define TRACKER_TRACE_STREAM frame_sequence_trace_
#define TRACKER_DCHECK_MSG                                      \
  " in " << GetFrameSequenceTrackerTypeName(this->type_)        \
         << " tracker: " << frame_sequence_trace_.str() << " (" \
         << frame_sequence_trace_.str().size() << ")";
#else
#define TRACKER_TRACE_STREAM EAT_STREAM_PARAMETERS
#define TRACKER_DCHECK_MSG ""
#endif

namespace cc {

const char* FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
    FrameSequenceTrackerType type) {
  switch (type) {
    case FrameSequenceTrackerType::kCompositorAnimation:
      return "CompositorAnimation";
    case FrameSequenceTrackerType::kMainThreadAnimation:
      return "MainThreadAnimation";
    case FrameSequenceTrackerType::kPinchZoom:
      return "PinchZoom";
    case FrameSequenceTrackerType::kRAF:
      return "RAF";
    case FrameSequenceTrackerType::kTouchScroll:
      return "TouchScroll";
    case FrameSequenceTrackerType::kUniversal:
      return "Universal";
    case FrameSequenceTrackerType::kVideo:
      return "Video";
    case FrameSequenceTrackerType::kWheelScroll:
      return "WheelScroll";
    case FrameSequenceTrackerType::kScrollbarScroll:
      return "ScrollbarScroll";
    case FrameSequenceTrackerType::kCustom:
      return "Custom";
    case FrameSequenceTrackerType::kMaxType:
      return "";
  }
}

namespace {

// Avoid reporting any throughput metric for sequences that do not have a
// sufficient number of frames.
constexpr int kMinFramesForThroughputMetric = 100;

constexpr int kBuiltinSequenceNum =
    static_cast<int>(FrameSequenceTrackerType::kMaxType) + 1;
constexpr int kMaximumHistogramIndex = 3 * kBuiltinSequenceNum;

int GetIndexForMetric(FrameSequenceMetrics::ThreadType thread_type,
                      FrameSequenceTrackerType type) {
  if (thread_type == FrameSequenceMetrics::ThreadType::kMain)
    return static_cast<int>(type);
  if (thread_type == FrameSequenceMetrics::ThreadType::kCompositor)
    return static_cast<int>(type) + kBuiltinSequenceNum;
  return static_cast<int>(type) + 2 * kBuiltinSequenceNum;
}

std::string GetCheckerboardingHistogramName(FrameSequenceTrackerType type) {
  return base::StrCat(
      {"Graphics.Smoothness.Checkerboarding.",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetThroughputHistogramName(FrameSequenceTrackerType type,
                                       const char* thread_name) {
  return base::StrCat(
      {"Graphics.Smoothness.PercentDroppedFrames.", thread_name, ".",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetFrameSequenceLengthHistogramName(FrameSequenceTrackerType type) {
  return base::StrCat(
      {"Graphics.Smoothness.FrameSequenceLength.",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

bool ShouldReportForAnimation(FrameSequenceTrackerType sequence_type,
                              FrameSequenceMetrics::ThreadType thread_type) {
  if (sequence_type == FrameSequenceTrackerType::kCompositorAnimation)
    return thread_type == FrameSequenceMetrics::ThreadType::kCompositor;

  if (sequence_type == FrameSequenceTrackerType::kMainThreadAnimation ||
      sequence_type == FrameSequenceTrackerType::kRAF)
    return thread_type == FrameSequenceMetrics::ThreadType::kMain;

  return false;
}

bool ShouldReportForInteraction(FrameSequenceMetrics* metrics,
                                FrameSequenceMetrics::ThreadType thread_type) {
  const auto sequence_type = metrics->type();

  // For touch/wheel scroll, the slower thread is the one we want to report. For
  // pinch-zoom, it's the compositor-thread.
  if (sequence_type == FrameSequenceTrackerType::kTouchScroll ||
      sequence_type == FrameSequenceTrackerType::kWheelScroll)
    return thread_type == metrics->GetEffectiveThread();

  if (sequence_type == FrameSequenceTrackerType::kPinchZoom)
    return thread_type == FrameSequenceMetrics::ThreadType::kCompositor;

  return false;
}

bool IsInteractionType(FrameSequenceTrackerType sequence_type) {
  return sequence_type == FrameSequenceTrackerType::kTouchScroll ||
         sequence_type == FrameSequenceTrackerType::kWheelScroll ||
         sequence_type == FrameSequenceTrackerType::kPinchZoom;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FrameSequenceMetrics

FrameSequenceMetrics::FrameSequenceMetrics(FrameSequenceTrackerType type,
                                           ThroughputUkmReporter* ukm_reporter)
    : type_(type),
      throughput_ukm_reporter_(ukm_reporter) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "cc,benchmark", "FrameSequenceTracker", TRACE_ID_LOCAL(this), "name",
      FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type_));
}

FrameSequenceMetrics::~FrameSequenceMetrics() {
  if (HasDataLeftForReporting()) {
    ReportMetrics();
  }
}

void FrameSequenceMetrics::SetScrollingThread(ThreadType scrolling_thread) {
  DCHECK(type_ == FrameSequenceTrackerType::kTouchScroll ||
         type_ == FrameSequenceTrackerType::kWheelScroll ||
         type_ == FrameSequenceTrackerType::kScrollbarScroll);
  DCHECK_EQ(scrolling_thread_, ThreadType::kUnknown);
  scrolling_thread_ = scrolling_thread;
}

FrameSequenceMetrics::ThreadType FrameSequenceMetrics::GetEffectiveThread()
    const {
  switch (type_) {
    case FrameSequenceTrackerType::kCompositorAnimation:
    case FrameSequenceTrackerType::kPinchZoom:
      return ThreadType::kCompositor;

    case FrameSequenceTrackerType::kMainThreadAnimation:
    case FrameSequenceTrackerType::kRAF:
    case FrameSequenceTrackerType::kVideo:
      return ThreadType::kMain;

    case FrameSequenceTrackerType::kTouchScroll:
    case FrameSequenceTrackerType::kScrollbarScroll:
    case FrameSequenceTrackerType::kWheelScroll:
      return scrolling_thread_;

    case FrameSequenceTrackerType::kUniversal:
      return ThreadType::kSlower;

    case FrameSequenceTrackerType::kCustom:
    case FrameSequenceTrackerType::kMaxType:
      NOTREACHED();
  }
  return ThreadType::kUnknown;
}

void FrameSequenceMetrics::Merge(
    std::unique_ptr<FrameSequenceMetrics> metrics) {
  DCHECK_EQ(type_, metrics->type_);
  DCHECK_EQ(GetEffectiveThread(), metrics->GetEffectiveThread());
  impl_throughput_.Merge(metrics->impl_throughput_);
  main_throughput_.Merge(metrics->main_throughput_);
  aggregated_throughput_.Merge(metrics->aggregated_throughput_);
  frames_checkerboarded_ += metrics->frames_checkerboarded_;

  // Reset the state of |metrics| before destroying it, so that it doesn't end
  // up reporting the metrics.
  metrics->impl_throughput_ = {};
  metrics->main_throughput_ = {};
  metrics->aggregated_throughput_ = {};
  metrics->frames_checkerboarded_ = 0;
}

bool FrameSequenceMetrics::HasEnoughDataForReporting() const {
  return impl_throughput_.frames_expected >= kMinFramesForThroughputMetric ||
         main_throughput_.frames_expected >= kMinFramesForThroughputMetric;
}

bool FrameSequenceMetrics::HasDataLeftForReporting() const {
  return impl_throughput_.frames_expected > 0 ||
         main_throughput_.frames_expected > 0;
}

void FrameSequenceMetrics::ComputeAggregatedThroughput() {
  // Whenever we are expecting and producing main frames, we are expecting and
  // producing impl frames as well. As an example, if we expect one main frame
  // to be produced, and when that main frame is presented, we are expecting 3
  // impl frames, then the number of expected frames is 3 for the aggregated
  // throughput.
  aggregated_throughput_.frames_expected = impl_throughput_.frames_expected;
  DCHECK_LE(aggregated_throughput_.frames_produced,
            aggregated_throughput_.frames_produced);
}

void FrameSequenceMetrics::ReportMetrics() {
  DCHECK_LE(impl_throughput_.frames_produced, impl_throughput_.frames_expected);
  DCHECK_LE(main_throughput_.frames_produced, main_throughput_.frames_expected);
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "cc,benchmark", "FrameSequenceTracker", TRACE_ID_LOCAL(this), "args",
      ThroughputData::ToTracedValue(impl_throughput_, main_throughput_),
      "checkerboard", frames_checkerboarded_);

  // Data for kCustom typed tracker is handled by caller instead being
  // reported here.
  if (type_ == FrameSequenceTrackerType::kCustom)
    return;

  ComputeAggregatedThroughput();

  // Report the throughput metrics.
  base::Optional<int> impl_throughput_percent = ThroughputData::ReportHistogram(
      this, ThreadType::kCompositor,
      GetIndexForMetric(FrameSequenceMetrics::ThreadType::kCompositor, type_),
      impl_throughput_);
  base::Optional<int> main_throughput_percent = ThroughputData::ReportHistogram(
      this, ThreadType::kMain,
      GetIndexForMetric(FrameSequenceMetrics::ThreadType::kMain, type_),
      main_throughput_);

  // Report for the 'slower thread' for the metrics where it makes sense.
  bool should_report_slower_thread =
      IsInteractionType(type_) || type_ == FrameSequenceTrackerType::kUniversal;
  base::Optional<int> aggregated_throughput_percent;
  if (should_report_slower_thread) {
    aggregated_throughput_percent = ThroughputData::ReportHistogram(
        this, ThreadType::kSlower,
        GetIndexForMetric(FrameSequenceMetrics::ThreadType::kSlower, type_),
        aggregated_throughput_);
    if (aggregated_throughput_percent.has_value() && throughput_ukm_reporter_) {
      throughput_ukm_reporter_->ReportThroughputUkm(
          aggregated_throughput_percent, impl_throughput_percent,
          main_throughput_percent, type_);
    }
  }

  // Report for the 'scrolling thread' for the scrolling interactions.
  if (scrolling_thread_ != ThreadType::kUnknown) {
    base::Optional<int> scrolling_thread_throughput;
    switch (scrolling_thread_) {
      case ThreadType::kCompositor:
        scrolling_thread_throughput = impl_throughput_percent;
        break;
      case ThreadType::kMain:
        scrolling_thread_throughput = main_throughput_percent;
        break;
      case ThreadType::kSlower:
      case ThreadType::kUnknown:
        NOTREACHED();
        break;
    }
    if (scrolling_thread_throughput.has_value()) {
      // It's OK to use the UMA histogram in the following code while still
      // using |GetThroughputHistogramName()| to get the name of the metric,
      // since the input-params to the function never change at runtime.
      if (type_ == FrameSequenceTrackerType::kWheelScroll) {
        UMA_HISTOGRAM_PERCENTAGE(
            GetThroughputHistogramName(FrameSequenceTrackerType::kWheelScroll,
                                       "ScrollingThread"),
            scrolling_thread_throughput.value());
      } else if (type_ == FrameSequenceTrackerType::kTouchScroll) {
        UMA_HISTOGRAM_PERCENTAGE(
            GetThroughputHistogramName(FrameSequenceTrackerType::kTouchScroll,
                                       "ScrollingThread"),
            scrolling_thread_throughput.value());
      } else {
        DCHECK_EQ(type_, FrameSequenceTrackerType::kScrollbarScroll);
        UMA_HISTOGRAM_PERCENTAGE(
            GetThroughputHistogramName(
                FrameSequenceTrackerType::kScrollbarScroll, "ScrollingThread"),
            scrolling_thread_throughput.value());
      }
    }
  }

  // Report the checkerboarding metrics.
  if (impl_throughput_.frames_expected >= kMinFramesForThroughputMetric) {
    const int checkerboarding_percent = static_cast<int>(
        100 * frames_checkerboarded_ / impl_throughput_.frames_expected);
    STATIC_HISTOGRAM_POINTER_GROUP(
        GetCheckerboardingHistogramName(type_), static_cast<int>(type_),
        static_cast<int>(FrameSequenceTrackerType::kMaxType),
        Add(checkerboarding_percent),
        base::LinearHistogram::FactoryGet(
            GetCheckerboardingHistogramName(type_), 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));
    frames_checkerboarded_ = 0;
  }

  // Reset the metrics that reach reporting threshold.
  if (impl_throughput_.frames_expected >= kMinFramesForThroughputMetric)
    impl_throughput_ = {};
  if (main_throughput_.frames_expected >= kMinFramesForThroughputMetric)
    main_throughput_ = {};
  if (aggregated_throughput_percent.has_value())
    aggregated_throughput_ = {};
}

////////////////////////////////////////////////////////////////////////////////
// FrameSequenceTrackerCollection

FrameSequenceTrackerCollection::FrameSequenceTrackerCollection(
    bool is_single_threaded,
    CompositorFrameReportingController* compositor_frame_reporting_controller)
    : is_single_threaded_(is_single_threaded),
      compositor_frame_reporting_controller_(
          compositor_frame_reporting_controller) {}

FrameSequenceTrackerCollection::~FrameSequenceTrackerCollection() {
  frame_trackers_.clear();
  removal_trackers_.clear();
}

FrameSequenceMetrics* FrameSequenceTrackerCollection::StartSequence(
    FrameSequenceTrackerType type) {
  DCHECK_NE(FrameSequenceTrackerType::kCustom, type);

  if (is_single_threaded_)
    return nullptr;
  if (frame_trackers_.contains(type))
    return frame_trackers_[type]->metrics();
  auto tracker = base::WrapUnique(
      new FrameSequenceTracker(type, throughput_ukm_reporter_.get()));
  frame_trackers_[type] = std::move(tracker);

  if (compositor_frame_reporting_controller_)
    compositor_frame_reporting_controller_->AddActiveTracker(type);
  return frame_trackers_[type]->metrics();
}

void FrameSequenceTrackerCollection::StopSequence(
    FrameSequenceTrackerType type) {
  DCHECK_NE(FrameSequenceTrackerType::kCustom, type);

  if (!frame_trackers_.contains(type))
    return;

  std::unique_ptr<FrameSequenceTracker> tracker =
      std::move(frame_trackers_[type]);

  if (compositor_frame_reporting_controller_)
    compositor_frame_reporting_controller_->RemoveActiveTracker(tracker->type_);

  frame_trackers_.erase(type);
  tracker->ScheduleTerminate();
  removal_trackers_.push_back(std::move(tracker));
}

void FrameSequenceTrackerCollection::StartCustomSequence(int sequence_id) {
  DCHECK(!base::Contains(custom_frame_trackers_, sequence_id));

  custom_frame_trackers_[sequence_id] = base::WrapUnique(
      new FrameSequenceTracker(FrameSequenceTrackerType::kCustom,
                               /*throughput_ukm_reporter=*/nullptr,
                               /*custom_sequence_id=*/sequence_id));
}

void FrameSequenceTrackerCollection::StopCustomSequence(int sequence_id) {
  auto it = custom_frame_trackers_.find(sequence_id);
  // This happens when an animation is aborted before starting.
  if (it == custom_frame_trackers_.end())
    return;

  std::unique_ptr<FrameSequenceTracker> tracker = std::move(it->second);
  custom_frame_trackers_.erase(it);
  tracker->ScheduleTerminate();
  removal_trackers_.push_back(std::move(tracker));
}

void FrameSequenceTrackerCollection::ClearAll() {
  frame_trackers_.clear();
  custom_frame_trackers_.clear();
  removal_trackers_.clear();
}

void FrameSequenceTrackerCollection::NotifyBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  RecreateTrackers(args);
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportBeginImplFrame(args);
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->ReportBeginImplFrame(args);
}

void FrameSequenceTrackerCollection::NotifyBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportBeginMainFrame(args);
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->ReportBeginMainFrame(args);
}

void FrameSequenceTrackerCollection::NotifyMainFrameProcessed(
    const viz::BeginFrameArgs& args) {
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportMainFrameProcessed(args);
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->ReportMainFrameProcessed(args);
}

void FrameSequenceTrackerCollection::NotifyImplFrameCausedNoDamage(
    const viz::BeginFrameAck& ack) {
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportImplFrameCausedNoDamage(ack);
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->ReportImplFrameCausedNoDamage(ack);
}

void FrameSequenceTrackerCollection::NotifyMainFrameCausedNoDamage(
    const viz::BeginFrameArgs& args) {
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportMainFrameCausedNoDamage(args);
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->ReportMainFrameCausedNoDamage(args);
}

void FrameSequenceTrackerCollection::NotifyPauseFrameProduction() {
  for (auto& tracker : frame_trackers_)
    tracker.second->PauseFrameProduction();
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->PauseFrameProduction();
}

void FrameSequenceTrackerCollection::NotifySubmitFrame(
    uint32_t frame_token,
    bool has_missing_content,
    const viz::BeginFrameAck& ack,
    const viz::BeginFrameArgs& origin_args) {
  for (auto& tracker : frame_trackers_) {
    tracker.second->ReportSubmitFrame(frame_token, has_missing_content, ack,
                                      origin_args);
  }
  for (auto& tracker : custom_frame_trackers_) {
    tracker.second->ReportSubmitFrame(frame_token, has_missing_content, ack,
                                      origin_args);
  }
}

void FrameSequenceTrackerCollection::NotifyFrameEnd(
    const viz::BeginFrameArgs& args,
    const viz::BeginFrameArgs& main_args) {
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportFrameEnd(args, main_args);
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->ReportFrameEnd(args, main_args);
}

void FrameSequenceTrackerCollection::NotifyFramePresented(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportFramePresented(frame_token, feedback);
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->ReportFramePresented(frame_token, feedback);

  for (auto& tracker : removal_trackers_)
    tracker->ReportFramePresented(frame_token, feedback);

  for (auto& tracker : removal_trackers_) {
    if (tracker->termination_status() ==
        FrameSequenceTracker::TerminationStatus::kReadyForTermination) {
      // The tracker is ready to be terminated.
      // For non kCustom typed trackers, take the metrics from the tracker.
      // merge with any outstanding metrics from previous trackers of the same
      // type. If there are enough frames to report the metrics, then report the
      // metrics and destroy it. Otherwise, retain it to be merged with
      // follow-up sequences.
      // For kCustom typed trackers, put its result in |custom_tracker_results_|
      // to be picked up by caller.
      auto metrics = tracker->TakeMetrics();
      if (tracker->type() == FrameSequenceTrackerType::kCustom) {
        custom_tracker_results_[tracker->custom_sequence_id()] =
            metrics->main_throughput();
        // |custom_tracker_results_| should be picked up timely.
        DCHECK_LT(custom_tracker_results_.size(), 500u);
        continue;
      }

      auto key = std::make_pair(tracker->type(), metrics->GetEffectiveThread());
      if (accumulated_metrics_.contains(key)) {
        metrics->Merge(std::move(accumulated_metrics_[key]));
        accumulated_metrics_.erase(key);
      }

#if DCHECK_IS_ON()
      // Handling the case like b(100)s(150)e(100)b(200)n(200), and then
      // StopSequence() is called which put this tracker in removal_trackers_.
      // Then P(150). In this case, frame 200 isn't processed yet, because this
      // no damage impl frame is considered 'processed' at e(200).
      const bool incomplete_frame_had_no_damage =
          !tracker->compositor_frame_submitted_ &&
          tracker->frame_had_no_compositor_damage_;
      if (tracker->is_inside_frame_ && incomplete_frame_had_no_damage)
        --metrics->impl_throughput().frames_received;

      if (metrics->impl_throughput().frames_received !=
          metrics->impl_throughput().frames_processed) {
        std::string output = tracker->frame_sequence_trace_.str().substr(
            tracker->ignored_trace_char_count_);
        NOTREACHED() << output;
      }
#endif
      if (metrics->HasEnoughDataForReporting())
        metrics->ReportMetrics();
      if (metrics->HasDataLeftForReporting())
        accumulated_metrics_[key] = std::move(metrics);
    }
  }

  // Destroy the trackers that are ready to be terminated.
  base::EraseIf(
      removal_trackers_,
      [](const std::unique_ptr<FrameSequenceTracker>& tracker) {
        return tracker->termination_status() ==
               FrameSequenceTracker::TerminationStatus::kReadyForTermination;
      });
}

void FrameSequenceTrackerCollection::RecreateTrackers(
    const viz::BeginFrameArgs& args) {
  std::vector<FrameSequenceTrackerType> recreate_trackers;
  for (const auto& tracker : frame_trackers_) {
    if (tracker.second->ShouldReportMetricsNow(args))
      recreate_trackers.push_back(tracker.first);
  }

  for (const auto& tracker_type : recreate_trackers) {
    // StopSequence put the tracker in the |removal_trackers_|, which will
    // report its throughput data when its frame is presented.
    StopSequence(tracker_type);
    // The frame sequence is still active, so create a new tracker to keep
    // tracking this sequence.
    StartSequence(tracker_type);
  }
}

ActiveFrameSequenceTrackers
FrameSequenceTrackerCollection::FrameSequenceTrackerActiveTypes() {
  ActiveFrameSequenceTrackers encoded_types = 0;
  for (const auto& tracker : frame_trackers_) {
    encoded_types |= static_cast<ActiveFrameSequenceTrackers>(
        1 << static_cast<unsigned>(tracker.first));
  }
  return encoded_types;
}

CustomTrackerResults
FrameSequenceTrackerCollection::TakeCustomTrackerResults() {
  return std::move(custom_tracker_results_);
}

FrameSequenceTracker* FrameSequenceTrackerCollection::GetTrackerForTesting(
    FrameSequenceTrackerType type) {
  if (!frame_trackers_.contains(type))
    return nullptr;
  return frame_trackers_[type].get();
}

void FrameSequenceTrackerCollection::SetUkmManager(UkmManager* manager) {
  DCHECK(frame_trackers_.empty());
  if (manager)
    throughput_ukm_reporter_ = std::make_unique<ThroughputUkmReporter>(manager);
  else
    throughput_ukm_reporter_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// FrameSequenceTracker

FrameSequenceTracker::FrameSequenceTracker(
    FrameSequenceTrackerType type,
    ThroughputUkmReporter* throughput_ukm_reporter,
    int custom_sequence_id)
    : type_(type),
      custom_sequence_id_(custom_sequence_id),
      metrics_(std::make_unique<FrameSequenceMetrics>(type,
                                                      throughput_ukm_reporter)),
      trace_data_(metrics_.get()) {
  DCHECK_LT(type_, FrameSequenceTrackerType::kMaxType);
  DCHECK(type_ != FrameSequenceTrackerType::kCustom ||
         custom_sequence_id_ >= 0);
}

FrameSequenceTracker::~FrameSequenceTracker() = default;

void FrameSequenceTracker::ScheduleTerminate() {
  termination_status_ = TerminationStatus::kScheduledForTermination;
  // It could happen that a main/impl frame is generated, but never processed
  // (didn't report no damage and didn't submit) when this happens.
  if (last_processed_impl_sequence_ < last_started_impl_sequence_) {
    DCHECK_GE(impl_throughput().frames_expected,
              begin_impl_frame_data_.previous_sequence_delta)
        << TRACKER_DCHECK_MSG;
    impl_throughput().frames_expected -=
        begin_impl_frame_data_.previous_sequence_delta;
#if DCHECK_IS_ON()
    --impl_throughput().frames_received;
#endif
  }
}

void FrameSequenceTracker::ReportBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "b(" << args.frame_id.sequence_number << ")";

#if DCHECK_IS_ON()
  DCHECK(!is_inside_frame_) << TRACKER_DCHECK_MSG;
  is_inside_frame_ = true;

  if (args.type == viz::BeginFrameArgs::NORMAL)
    impl_frames_.insert(args.frame_id);
#endif

  DCHECK_EQ(last_started_impl_sequence_, 0u) << TRACKER_DCHECK_MSG;
  last_started_impl_sequence_ = args.frame_id.sequence_number;
  if (reset_all_state_) {
    begin_impl_frame_data_ = {};
    begin_main_frame_data_ = {};
    reset_all_state_ = false;
  }

  DCHECK(!frame_had_no_compositor_damage_) << TRACKER_DCHECK_MSG;
  DCHECK(!compositor_frame_submitted_) << TRACKER_DCHECK_MSG;

  UpdateTrackedFrameData(&begin_impl_frame_data_, args.frame_id.source_id,
                         args.frame_id.sequence_number);
  impl_throughput().frames_expected +=
      begin_impl_frame_data_.previous_sequence_delta;
#if DCHECK_IS_ON()
  ++impl_throughput().frames_received;
#endif

  if (first_frame_timestamp_.is_null())
    first_frame_timestamp_ = args.frame_time;
}

void FrameSequenceTracker::ReportBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "B(" << begin_main_frame_data_.previous_sequence
                       << "," << args.frame_id.sequence_number << ")";

  if (first_received_main_sequence_ &&
      first_received_main_sequence_ > args.frame_id.sequence_number) {
    return;
  }

  if (!first_received_main_sequence_ &&
      ShouldIgnoreSequence(args.frame_id.sequence_number)) {
    return;
  }

#if DCHECK_IS_ON()
  if (args.type == viz::BeginFrameArgs::NORMAL) {
    DCHECK(impl_frames_.contains(args.frame_id)) << TRACKER_DCHECK_MSG;
  }
#endif

  DCHECK_EQ(awaiting_main_response_sequence_, 0u) << TRACKER_DCHECK_MSG;
  last_processed_main_sequence_latency_ = 0;
  awaiting_main_response_sequence_ = args.frame_id.sequence_number;

  UpdateTrackedFrameData(&begin_main_frame_data_, args.frame_id.source_id,
                         args.frame_id.sequence_number);
  if (!first_received_main_sequence_ ||
      first_received_main_sequence_ <= last_no_main_damage_sequence_) {
    first_received_main_sequence_ = args.frame_id.sequence_number;
  }
  main_throughput().frames_expected +=
      begin_main_frame_data_.previous_sequence_delta;
  previous_begin_main_sequence_ = current_begin_main_sequence_;
  current_begin_main_sequence_ = args.frame_id.sequence_number;
}

void FrameSequenceTracker::ReportMainFrameProcessed(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "E(" << args.frame_id.sequence_number << ")";

  const bool previous_main_frame_submitted_or_no_damage =
      previous_begin_main_sequence_ != 0 &&
      (last_submitted_main_sequence_ == previous_begin_main_sequence_ ||
       last_no_main_damage_sequence_ == previous_begin_main_sequence_);
  if (last_processed_main_sequence_ != 0 &&
      !had_impl_frame_submitted_between_commits_ &&
      !previous_main_frame_submitted_or_no_damage) {
    DCHECK_GE(main_throughput().frames_expected,
              begin_main_frame_data_.previous_sequence_delta)
        << TRACKER_DCHECK_MSG;
    main_throughput().frames_expected -=
        begin_main_frame_data_.previous_sequence_delta;
    last_no_main_damage_sequence_ = previous_begin_main_sequence_;
  }
  had_impl_frame_submitted_between_commits_ = false;

  if (first_received_main_sequence_ &&
      args.frame_id.sequence_number >= first_received_main_sequence_) {
    if (awaiting_main_response_sequence_) {
      DCHECK_EQ(awaiting_main_response_sequence_, args.frame_id.sequence_number)
          << TRACKER_DCHECK_MSG;
    }
    DCHECK_EQ(last_processed_main_sequence_latency_, 0u) << TRACKER_DCHECK_MSG;
    last_processed_main_sequence_ = args.frame_id.sequence_number;
    last_processed_main_sequence_latency_ =
        std::max(last_started_impl_sequence_, last_processed_impl_sequence_) -
        args.frame_id.sequence_number;
    awaiting_main_response_sequence_ = 0;
  }
}

void FrameSequenceTracker::ReportSubmitFrame(
    uint32_t frame_token,
    bool has_missing_content,
    const viz::BeginFrameAck& ack,
    const viz::BeginFrameArgs& origin_args) {
  if (termination_status_ != TerminationStatus::kActive ||
      ShouldIgnoreBeginFrameSource(ack.frame_id.source_id) ||
      ShouldIgnoreSequence(ack.frame_id.sequence_number)) {
    ignored_frame_tokens_.insert(frame_token);
    return;
  }

#if DCHECK_IS_ON()
  DCHECK(is_inside_frame_) << TRACKER_DCHECK_MSG;
  DCHECK_LT(impl_throughput().frames_processed,
            impl_throughput().frames_received)
      << TRACKER_DCHECK_MSG;
  ++impl_throughput().frames_processed;
#endif

  last_processed_impl_sequence_ = ack.frame_id.sequence_number;
  if (first_submitted_frame_ == 0)
    first_submitted_frame_ = frame_token;
  last_submitted_frame_ = frame_token;
  compositor_frame_submitted_ = true;

  TRACKER_TRACE_STREAM << "s(" << frame_token << ")";
  had_impl_frame_submitted_between_commits_ = true;

  const bool main_changes_after_sequence_started =
      first_received_main_sequence_ &&
      origin_args.frame_id.sequence_number >= first_received_main_sequence_;
  const bool main_changes_include_new_changes =
      last_submitted_main_sequence_ == 0 ||
      origin_args.frame_id.sequence_number > last_submitted_main_sequence_;
  const bool main_change_had_no_damage =
      last_no_main_damage_sequence_ != 0 &&
      origin_args.frame_id.sequence_number == last_no_main_damage_sequence_;

  if (!ShouldIgnoreBeginFrameSource(origin_args.frame_id.source_id)) {
    if (main_changes_after_sequence_started &&
        main_changes_include_new_changes && !main_change_had_no_damage) {
      submitted_frame_had_new_main_content_ = true;
      TRACKER_TRACE_STREAM << "S(" << origin_args.frame_id.sequence_number
                           << ")";

      last_submitted_main_sequence_ = origin_args.frame_id.sequence_number;
      main_frames_.push_back(frame_token);
      DCHECK_GE(main_throughput().frames_expected, main_frames_.size())
          << TRACKER_DCHECK_MSG;
    } else {
      // If we have sent a BeginMainFrame which hasn't yet been submitted, or
      // confirmed that it has no damage (previous_sequence is set to 0), then
      // we are currently expecting a main frame.
      const bool expecting_main = begin_main_frame_data_.previous_sequence >
                                  last_submitted_main_sequence_;
      if (expecting_main)
        expecting_main_when_submit_impl_.push_back(frame_token);
    }
  }

  if (has_missing_content) {
    checkerboarding_.frames.push_back(frame_token);
  }
}

void FrameSequenceTracker::ReportFrameEnd(
    const viz::BeginFrameArgs& args,
    const viz::BeginFrameArgs& main_args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "e(" << args.frame_id.sequence_number << ","
                       << main_args.frame_id.sequence_number << ")";

  bool should_ignore_sequence =
      ShouldIgnoreSequence(args.frame_id.sequence_number);
  if (reset_all_state_) {
    begin_impl_frame_data_ = {};
    begin_main_frame_data_ = {};
    reset_all_state_ = false;
  }

  if (should_ignore_sequence) {
#if DCHECK_IS_ON()
    is_inside_frame_ = false;
#endif
    return;
  }

  if (compositor_frame_submitted_ && submitted_frame_had_new_main_content_ &&
      last_processed_main_sequence_latency_) {
    // If a compositor frame was submitted with new content from the
    // main-thread, then make sure the latency gets accounted for.
    main_throughput().frames_expected += last_processed_main_sequence_latency_;
  }

  // It is possible that the compositor claims there was no damage from the
  // compositor, but before the frame ends, it submits a compositor frame (e.g.
  // with some damage from main). In such cases, the compositor is still
  // responsible for processing the update, and therefore the 'no damage' claim
  // is ignored.
  if (frame_had_no_compositor_damage_ && !compositor_frame_submitted_) {
    DCHECK_GT(impl_throughput().frames_expected, 0u) << TRACKER_DCHECK_MSG;
    DCHECK_GT(impl_throughput().frames_expected,
              impl_throughput().frames_produced)
        << TRACKER_DCHECK_MSG;
    --impl_throughput().frames_expected;
#if DCHECK_IS_ON()
    ++impl_throughput().frames_processed;
    // If these two are the same, it means that each impl frame is either
    // no-damage or submitted. That's expected, so we don't need those in the
    // output of DCHECK.
    if (impl_throughput().frames_processed == impl_throughput().frames_received)
      ignored_trace_char_count_ = frame_sequence_trace_.str().size();
    else
      NOTREACHED() << TRACKER_DCHECK_MSG;
#endif
    begin_impl_frame_data_.previous_sequence = 0;
  }
  frame_had_no_compositor_damage_ = false;
  compositor_frame_submitted_ = false;
  submitted_frame_had_new_main_content_ = false;
  last_processed_main_sequence_latency_ = 0;

#if DCHECK_IS_ON()
  DCHECK(is_inside_frame_) << TRACKER_DCHECK_MSG;
  is_inside_frame_ = false;
#endif

  DCHECK_EQ(last_started_impl_sequence_, last_processed_impl_sequence_)
      << TRACKER_DCHECK_MSG;
  last_started_impl_sequence_ = 0;
}

void FrameSequenceTracker::ReportFramePresented(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  const bool frame_token_acks_last_frame =
      frame_token == last_submitted_frame_ ||
      viz::FrameTokenGT(frame_token, last_submitted_frame_);

  // Update termination status if this is scheduled for termination, and it is
  // not waiting for any frames, or it has received the presentation-feedback
  // for the latest frame it is tracking.
  if (termination_status_ == TerminationStatus::kScheduledForTermination &&
      (last_submitted_frame_ == 0 || frame_token_acks_last_frame)) {
    termination_status_ = TerminationStatus::kReadyForTermination;
  }

  if (first_submitted_frame_ == 0 ||
      viz::FrameTokenGT(first_submitted_frame_, frame_token)) {
    // We are getting presentation feedback for frames that were submitted
    // before this sequence started. So ignore these.
    return;
  }

  TRACKER_TRACE_STREAM << "P(" << frame_token << ")";

  if (ignored_frame_tokens_.contains(frame_token))
    return;

  uint32_t impl_frames_produced = 0;
  uint32_t main_frames_produced = 0;
  trace_data_.Advance(feedback.timestamp);

  const bool was_presented = !feedback.timestamp.is_null();
  if (was_presented && last_submitted_frame_) {
    DCHECK_LT(impl_throughput().frames_produced,
              impl_throughput().frames_expected)
        << TRACKER_DCHECK_MSG;
    ++impl_throughput().frames_produced;
    ++impl_frames_produced;

    if (frame_token_acks_last_frame)
      last_submitted_frame_ = 0;
  }

  if (was_presented) {
    // This presentation includes the visual update from all main frame tokens
    // <= |frame_token|.
    const unsigned size_before_erase = main_frames_.size();
    while (!main_frames_.empty() &&
           !viz::FrameTokenGT(main_frames_.front(), frame_token)) {
      main_frames_.pop_front();
    }
    if (main_frames_.size() < size_before_erase) {
      DCHECK_LT(main_throughput().frames_produced,
                main_throughput().frames_expected)
          << TRACKER_DCHECK_MSG;
      ++main_throughput().frames_produced;
      ++main_frames_produced;
    }

    if (impl_frames_produced > 0) {
      // If there is no main frame presented, then we need to see whether or not
      // we are expecting main frames to be presented or not.
      if (main_frames_produced == 0) {
        // Only need to check the first element in the deque because the
        // elements are in order.
        bool expecting_main_frames =
            !expecting_main_when_submit_impl_.empty() &&
            !viz::FrameTokenGT(expecting_main_when_submit_impl_[0],
                               frame_token);
        if (expecting_main_frames) {
          // We are expecting a main frame to be processed, the main frame
          // should either report no-damage or be submitted to GPU. Since we
          // don't know which case it would be, we accumulate the number of impl
          // frames produced so that we can apply that to aggregated throughput
          // if the main frame reports no-damage later on.
          impl_frames_produced_while_expecting_main_ += impl_frames_produced;
        } else {
          DCHECK_EQ(impl_frames_produced_while_expecting_main_, 0u)
              << TRACKER_DCHECK_MSG;
          aggregated_throughput().frames_produced += impl_frames_produced;
          impl_frames_produced_while_expecting_main_ = 0;
        }
      } else {
        aggregated_throughput().frames_produced += main_frames_produced;
        impl_frames_produced_while_expecting_main_ = 0;
        while (!expecting_main_when_submit_impl_.empty() &&
               !viz::FrameTokenGT(expecting_main_when_submit_impl_.front(),
                                  frame_token)) {
          expecting_main_when_submit_impl_.pop_front();
        }
      }
    }

    if (checkerboarding_.last_frame_had_checkerboarding) {
      DCHECK(!checkerboarding_.last_frame_timestamp.is_null())
          << TRACKER_DCHECK_MSG;
      DCHECK(!feedback.timestamp.is_null()) << TRACKER_DCHECK_MSG;

      // |feedback.timestamp| is the timestamp when the latest frame was
      // presented. |checkerboarding_.last_frame_timestamp| is the timestamp
      // when the previous frame (which had checkerboarding) was presented. Use
      // |feedback.interval| to compute the number of vsyncs that have passed
      // between the two frames (since that is how many times the user saw that
      // checkerboarded frame).
      base::TimeDelta difference =
          feedback.timestamp - checkerboarding_.last_frame_timestamp;
      const auto& interval = feedback.interval.is_zero()
                                 ? viz::BeginFrameArgs::DefaultInterval()
                                 : feedback.interval;
      DCHECK(!interval.is_zero()) << TRACKER_DCHECK_MSG;
      constexpr base::TimeDelta kEpsilon = base::TimeDelta::FromMilliseconds(1);
      int64_t frames = (difference + kEpsilon) / interval;
      metrics_->add_checkerboarded_frames(frames);
    }

    const bool frame_had_checkerboarding =
        base::Contains(checkerboarding_.frames, frame_token);
    checkerboarding_.last_frame_had_checkerboarding = frame_had_checkerboarding;
    checkerboarding_.last_frame_timestamp = feedback.timestamp;
  }

  while (!checkerboarding_.frames.empty() &&
         !viz::FrameTokenGT(checkerboarding_.frames.front(), frame_token)) {
    checkerboarding_.frames.pop_front();
  }
}

void FrameSequenceTracker::ReportImplFrameCausedNoDamage(
    const viz::BeginFrameAck& ack) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(ack.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "n(" << ack.frame_id.sequence_number << ")";

  // It is possible that this is called before a begin-impl-frame has been
  // dispatched for this frame-sequence. In such cases, ignore this call.
  if (ShouldIgnoreSequence(ack.frame_id.sequence_number))
    return;

  last_processed_impl_sequence_ = ack.frame_id.sequence_number;
  // If there is no damage for this frame (and no frame is submitted), then the
  // impl-sequence needs to be reset. However, this should be done after the
  // processing the frame is complete (i.e. in ReportFrameEnd()), so that other
  // notifications (e.g. 'no main damage' etc.) can be handled correctly.
  DCHECK_EQ(begin_impl_frame_data_.previous_sequence,
            ack.frame_id.sequence_number);
  frame_had_no_compositor_damage_ = true;
}

void FrameSequenceTracker::ReportMainFrameCausedNoDamage(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "N(" << begin_main_frame_data_.previous_sequence
                       << "," << args.frame_id.sequence_number << ")";

  if (!first_received_main_sequence_ ||
      first_received_main_sequence_ > args.frame_id.sequence_number) {
    return;
  }

  if (last_no_main_damage_sequence_ == args.frame_id.sequence_number)
    return;

  // It is possible for |awaiting_main_response_sequence_| to be zero here if a
  // commit had already happened before (e.g. B(x)E(x)N(x)). So check that case
  // here.
  if (awaiting_main_response_sequence_) {
    DCHECK_EQ(awaiting_main_response_sequence_, args.frame_id.sequence_number)
        << TRACKER_DCHECK_MSG;
  } else {
    DCHECK_EQ(last_processed_main_sequence_, args.frame_id.sequence_number)
        << TRACKER_DCHECK_MSG;
  }
  awaiting_main_response_sequence_ = 0;

  DCHECK_GT(main_throughput().frames_expected, 0u) << TRACKER_DCHECK_MSG;
  DCHECK_GT(main_throughput().frames_expected,
            main_throughput().frames_produced)
      << TRACKER_DCHECK_MSG;
  last_no_main_damage_sequence_ = args.frame_id.sequence_number;
  --main_throughput().frames_expected;
  DCHECK_GE(main_throughput().frames_expected, main_frames_.size())
      << TRACKER_DCHECK_MSG;

  // Could be 0 if there were a pause frame production.
  if (begin_main_frame_data_.previous_sequence != 0) {
    DCHECK_EQ(begin_main_frame_data_.previous_sequence,
              args.frame_id.sequence_number)
        << TRACKER_DCHECK_MSG;
  }
  begin_main_frame_data_.previous_sequence = 0;

  aggregated_throughput().frames_produced +=
      impl_frames_produced_while_expecting_main_;
  impl_frames_produced_while_expecting_main_ = 0;
  expecting_main_when_submit_impl_.clear();
}

void FrameSequenceTracker::PauseFrameProduction() {
  // The states need to be reset, so that the tracker ignores the vsyncs until
  // the next received begin-frame. However, defer doing that until the frame
  // ends (or a new frame starts), so that in case a frame is in-progress,
  // subsequent notifications for that frame can be handled correctly.
  TRACKER_TRACE_STREAM << 'R';
  reset_all_state_ = true;
}

void FrameSequenceTracker::UpdateTrackedFrameData(TrackedFrameData* frame_data,
                                                  uint64_t source_id,
                                                  uint64_t sequence_number) {
  if (frame_data->previous_sequence &&
      frame_data->previous_source == source_id) {
    uint32_t current_latency = sequence_number - frame_data->previous_sequence;
    DCHECK_GT(current_latency, 0u) << TRACKER_DCHECK_MSG;
    frame_data->previous_sequence_delta = current_latency;
  } else {
    frame_data->previous_sequence_delta = 1;
  }
  frame_data->previous_source = source_id;
  frame_data->previous_sequence = sequence_number;
}

bool FrameSequenceTracker::ShouldIgnoreBeginFrameSource(
    uint64_t source_id) const {
  if (begin_impl_frame_data_.previous_source == 0)
    return source_id == viz::BeginFrameArgs::kManualSourceId;
  return source_id != begin_impl_frame_data_.previous_source;
}

// This check ensures that when ReportBeginMainFrame, or ReportSubmitFrame, or
// ReportFramePresented is called for a particular arg, the ReportBeginImplFrame
// is been called already.
bool FrameSequenceTracker::ShouldIgnoreSequence(
    uint64_t sequence_number) const {
  return begin_impl_frame_data_.previous_sequence == 0 ||
         sequence_number < begin_impl_frame_data_.previous_sequence;
}

std::unique_ptr<base::trace_event::TracedValue>
FrameSequenceMetrics::ThroughputData::ToTracedValue(
    const ThroughputData& impl,
    const ThroughputData& main) {
  auto dict = std::make_unique<base::trace_event::TracedValue>();
  dict->SetInteger("impl-frames-produced", impl.frames_produced);
  dict->SetInteger("impl-frames-expected", impl.frames_expected);
  dict->SetInteger("main-frames-produced", main.frames_produced);
  dict->SetInteger("main-frames-expected", main.frames_expected);
  return dict;
}

bool FrameSequenceTracker::ShouldReportMetricsNow(
    const viz::BeginFrameArgs& args) const {
  return metrics_->HasEnoughDataForReporting() &&
         !first_frame_timestamp_.is_null() &&
         args.frame_time - first_frame_timestamp_ >= time_delta_to_report_;
}

std::unique_ptr<FrameSequenceMetrics> FrameSequenceTracker::TakeMetrics() {
  return std::move(metrics_);
}

base::Optional<int> FrameSequenceMetrics::ThroughputData::ReportHistogram(
    FrameSequenceMetrics* metrics,
    ThreadType thread_type,
    int metric_index,
    const ThroughputData& data) {
  const auto sequence_type = metrics->type();
  DCHECK_LT(sequence_type, FrameSequenceTrackerType::kMaxType);

  STATIC_HISTOGRAM_POINTER_GROUP(
      GetFrameSequenceLengthHistogramName(sequence_type),
      static_cast<int>(sequence_type),
      static_cast<int>(FrameSequenceTrackerType::kMaxType),
      Add(data.frames_expected),
      base::Histogram::FactoryGet(
          GetFrameSequenceLengthHistogramName(sequence_type), 1, 1000, 50,
          base::HistogramBase::kUmaTargetedHistogramFlag));

  if (data.frames_expected < kMinFramesForThroughputMetric)
    return base::nullopt;

  // Throughput means the percent of frames that was expected to show on the
  // screen but didn't. In other words, the lower the throughput is, the
  // smoother user experience.
  const int percent =
      std::ceil(100 * (data.frames_expected - data.frames_produced) /
                static_cast<double>(data.frames_expected));

  const bool is_animation =
      ShouldReportForAnimation(sequence_type, thread_type);
  const bool is_interaction = ShouldReportForInteraction(metrics, thread_type);

  ThroughputUkmReporter* const ukm_reporter = metrics->ukm_reporter();

  if (is_animation) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentDroppedFrames.AllAnimations", percent);
    if (ukm_reporter) {
      ukm_reporter->ReportAggregateThroughput(AggregationType::kAllAnimations,
                                              percent);
    }
  }

  if (is_interaction) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentDroppedFrames.AllInteractions", percent);
    if (ukm_reporter) {
      ukm_reporter->ReportAggregateThroughput(AggregationType::kAllInteractions,
                                              percent);
    }
  }

  if (is_animation || is_interaction) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentDroppedFrames.AllSequences", percent);
    if (ukm_reporter) {
      ukm_reporter->ReportAggregateThroughput(AggregationType::kAllSequences,
                                              percent);
    }
  }

  if (!is_animation && !IsInteractionType(sequence_type) &&
      sequence_type != FrameSequenceTrackerType::kUniversal &&
      sequence_type != FrameSequenceTrackerType::kVideo) {
    return base::nullopt;
  }

  const char* thread_name =
      thread_type == ThreadType::kCompositor
          ? "CompositorThread"
          : thread_type == ThreadType::kMain ? "MainThread" : "SlowerThread";
  STATIC_HISTOGRAM_POINTER_GROUP(
      GetThroughputHistogramName(sequence_type, thread_name), metric_index,
      kMaximumHistogramIndex, Add(percent),
      base::LinearHistogram::FactoryGet(
          GetThroughputHistogramName(sequence_type, thread_name), 1, 100, 101,
          base::HistogramBase::kUmaTargetedHistogramFlag));
  return percent;
}

FrameSequenceTracker::CheckerboardingData::CheckerboardingData() = default;
FrameSequenceTracker::CheckerboardingData::~CheckerboardingData() = default;

FrameSequenceTracker::TraceData::TraceData(const void* id) : trace_id(id) {}
void FrameSequenceTracker::TraceData::Advance(base::TimeTicks new_timestamp) {
  // Use different names, because otherwise the trace-viewer shows the slices in
  // the same color, and that makes it difficult to tell the traces apart from
  // each other.
  const char* trace_names[] = {"Frame", "Frame ", "Frame   "};
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "cc,benchmark", trace_names[++this->frame_count % 3],
      TRACE_ID_LOCAL(this->trace_id), this->last_timestamp);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "cc,benchmark", trace_names[this->frame_count % 3],
      TRACE_ID_LOCAL(this->trace_id), new_timestamp);
  this->last_timestamp = new_timestamp;
}

}  // namespace cc
