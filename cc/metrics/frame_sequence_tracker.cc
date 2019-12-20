// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_tracker.h"

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
#define TRACKER_DCHECK_MSG                                                 \
  " in " << GetFrameSequenceTrackerTypeName(static_cast<int>(this->type_)) \
         << " tracker: " << frame_sequence_trace_.str() << " ("            \
         << frame_sequence_trace_.str().size() << ")";
#else
#define TRACKER_TRACE_STREAM EAT_STREAM_PARAMETERS
#define TRACKER_DCHECK_MSG ""
#endif

namespace cc {

const char* FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
    int type_index) {
  switch (type_index) {
    case static_cast<int>(FrameSequenceTrackerType::kCompositorAnimation):
      return "CompositorAnimation";
    case static_cast<int>(FrameSequenceTrackerType::kMainThreadAnimation):
      return "MainThreadAnimation";
    case static_cast<int>(FrameSequenceTrackerType::kPinchZoom):
      return "PinchZoom";
    case static_cast<int>(FrameSequenceTrackerType::kRAF):
      return "RAF";
    case static_cast<int>(FrameSequenceTrackerType::kTouchScroll):
      return "TouchScroll";
    case static_cast<int>(FrameSequenceTrackerType::kUniversal):
      return "Universal";
    case static_cast<int>(FrameSequenceTrackerType::kVideo):
      return "Video";
    case static_cast<int>(FrameSequenceTrackerType::kWheelScroll):
      return "WheelScroll";
    default:
      return "";
  }
}

namespace {

// Avoid reporting any throughput metric for sequences that do not have a
// sufficient number of frames.
constexpr int kMinFramesForThroughputMetric = 100;

constexpr int kBuiltinSequenceNum = FrameSequenceTrackerType::kMaxType + 1;
constexpr int kMaximumHistogramIndex = 3 * kBuiltinSequenceNum;

int GetIndexForMetric(FrameSequenceMetrics::ThreadType thread_type,
                      FrameSequenceTrackerType type) {
  if (thread_type == FrameSequenceMetrics::ThreadType::kMain)
    return static_cast<int>(type);
  if (thread_type == FrameSequenceMetrics::ThreadType::kCompositor)
    return static_cast<int>(type + kBuiltinSequenceNum);
  return static_cast<int>(type + 2 * kBuiltinSequenceNum);
}

std::string GetCheckerboardingHistogramName(FrameSequenceTrackerType type) {
  return base::StrCat({"Graphics.Smoothness.Checkerboarding.",
                       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
                           static_cast<int>(type))});
}

std::string GetThroughputHistogramName(FrameSequenceTrackerType type,
                                       const char* thread_name) {
  return base::StrCat({"Graphics.Smoothness.Throughput.", thread_name, ".",
                       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
                           static_cast<int>(type))});
}

std::string GetFrameSequenceLengthHistogramName(FrameSequenceTrackerType type) {
  return base::StrCat({"Graphics.Smoothness.FrameSequenceLength.",
                       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
                           static_cast<int>(type))});
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

bool ShouldReportForInteraction(FrameSequenceTrackerType sequence_type,
                                FrameSequenceMetrics::ThreadType thread_type) {
  // For touch/wheel scroll, the slower thread is the one we want to report. For
  // pinch-zoom, it's the compositor-thread.
  if (sequence_type == FrameSequenceTrackerType::kTouchScroll ||
      sequence_type == FrameSequenceTrackerType::kWheelScroll)
    return thread_type == FrameSequenceMetrics::ThreadType::kSlower;

  if (sequence_type == FrameSequenceTrackerType::kPinchZoom)
    return thread_type == FrameSequenceMetrics::ThreadType::kCompositor;

  return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FrameSequenceMetrics

FrameSequenceMetrics::FrameSequenceMetrics(FrameSequenceTrackerType type,
                                           UkmManager* ukm_manager,
                                           ThroughputUkmReporter* ukm_reporter)
    : type_(type),
      ukm_manager_(ukm_manager),
      throughput_ukm_reporter_(ukm_reporter) {
  TRACE_EVENT_ASYNC_BEGIN1(
      "cc,benchmark", "FrameSequenceTracker", this, "name",
      FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
          static_cast<int>(type_)));
}

FrameSequenceMetrics::~FrameSequenceMetrics() {
  if (HasDataLeftForReporting())
    ReportMetrics();
}

void FrameSequenceMetrics::Merge(
    std::unique_ptr<FrameSequenceMetrics> metrics) {
  DCHECK_EQ(type_, metrics->type_);
  impl_throughput_.Merge(metrics->impl_throughput_);
  main_throughput_.Merge(metrics->main_throughput_);
  frames_checkerboarded_ += metrics->frames_checkerboarded_;

  // Reset the state of |metrics| before destroying it, so that it doesn't end
  // up reporting the metrics.
  metrics->impl_throughput_ = {};
  metrics->main_throughput_ = {};
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

void FrameSequenceMetrics::ReportMetrics() {
  DCHECK_LE(impl_throughput_.frames_produced, impl_throughput_.frames_expected);
  DCHECK_LE(main_throughput_.frames_produced, main_throughput_.frames_expected);
  TRACE_EVENT_ASYNC_END2(
      "cc,benchmark", "FrameSequenceTracker", this, "args",
      ThroughputData::ToTracedValue(impl_throughput_, main_throughput_),
      "checkerboard", frames_checkerboarded_);

  // Report the throughput metrics.
  base::Optional<int> impl_throughput_percent = ThroughputData::ReportHistogram(
      type_, ThreadType::kCompositor,
      GetIndexForMetric(FrameSequenceMetrics::ThreadType::kCompositor, type_),
      impl_throughput_);
  base::Optional<int> main_throughput_percent = ThroughputData::ReportHistogram(
      type_, ThreadType::kMain,
      GetIndexForMetric(FrameSequenceMetrics::ThreadType::kMain, type_),
      main_throughput_);

  base::Optional<ThroughputData> slower_throughput;
  base::Optional<int> slower_throughput_percent;
  if (impl_throughput_percent &&
      (!main_throughput_percent ||
       impl_throughput_percent.value() <= main_throughput_percent.value())) {
    slower_throughput = impl_throughput_;
  }
  if (main_throughput_percent &&
      (!impl_throughput_percent ||
       main_throughput_percent.value() < impl_throughput_percent.value())) {
    slower_throughput = main_throughput_;
  }
  if (slower_throughput.has_value()) {
    slower_throughput_percent = ThroughputData::ReportHistogram(
        type_, ThreadType::kSlower,
        GetIndexForMetric(FrameSequenceMetrics::ThreadType::kSlower, type_),
        slower_throughput.value());
    DCHECK(slower_throughput_percent.has_value());
  }

  // slower_throughput has value indicates that we have reported UMA.
  if (slower_throughput.has_value() && ukm_manager_ &&
      throughput_ukm_reporter_) {
    throughput_ukm_reporter_->ReportThroughputUkm(
        ukm_manager_, slower_throughput_percent, impl_throughput_percent,
        main_throughput_percent, type_);
  }

  // Report the checkerboarding metrics.
  if (impl_throughput_.frames_expected >= kMinFramesForThroughputMetric) {
    const int checkerboarding_percent = static_cast<int>(
        100 * frames_checkerboarded_ / impl_throughput_.frames_expected);
    STATIC_HISTOGRAM_POINTER_GROUP(
        GetCheckerboardingHistogramName(type_), type_,
        FrameSequenceTrackerType::kMaxType, Add(checkerboarding_percent),
        base::LinearHistogram::FactoryGet(
            GetCheckerboardingHistogramName(type_), 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));
    frames_checkerboarded_ = 0;
  }

  // Reset the metrics that have already been reported.
  if (impl_throughput_percent.has_value())
    impl_throughput_ = {};
  if (main_throughput_percent.has_value())
    main_throughput_ = {};
}

////////////////////////////////////////////////////////////////////////////////
// FrameSequenceTrackerCollection

FrameSequenceTrackerCollection::FrameSequenceTrackerCollection(
    bool is_single_threaded,
    CompositorFrameReportingController* compositor_frame_reporting_controller)
    : is_single_threaded_(is_single_threaded),
      compositor_frame_reporting_controller_(
          compositor_frame_reporting_controller),
      throughput_ukm_reporter_(std::make_unique<ThroughputUkmReporter>()) {}

FrameSequenceTrackerCollection::~FrameSequenceTrackerCollection() {
  frame_trackers_.clear();
  removal_trackers_.clear();
}

void FrameSequenceTrackerCollection::StartSequence(
    FrameSequenceTrackerType type) {
  if (is_single_threaded_)
    return;
  if (frame_trackers_.contains(type))
    return;
  auto tracker = base::WrapUnique(new FrameSequenceTracker(
      type, ukm_manager_, throughput_ukm_reporter_.get()));
  frame_trackers_[type] = std::move(tracker);

  if (compositor_frame_reporting_controller_)
    compositor_frame_reporting_controller_->AddActiveTracker(type);
}

void FrameSequenceTrackerCollection::StopSequence(
    FrameSequenceTrackerType type) {
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

void FrameSequenceTrackerCollection::ClearAll() {
  frame_trackers_.clear();
  removal_trackers_.clear();
}

void FrameSequenceTrackerCollection::NotifyBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  RecreateTrackers(args);
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportBeginImplFrame(args);
}

void FrameSequenceTrackerCollection::NotifyBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportBeginMainFrame(args);
}

void FrameSequenceTrackerCollection::NotifyImplFrameCausedNoDamage(
    const viz::BeginFrameAck& ack) {
  for (auto& tracker : frame_trackers_) {
    tracker.second->ReportImplFrameCausedNoDamage(ack);
  }
}

void FrameSequenceTrackerCollection::NotifyMainFrameCausedNoDamage(
    const viz::BeginFrameArgs& args) {
  for (auto& tracker : frame_trackers_) {
    tracker.second->ReportMainFrameCausedNoDamage(args);
  }
}

void FrameSequenceTrackerCollection::NotifyPauseFrameProduction() {
  for (auto& tracker : frame_trackers_)
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
}

void FrameSequenceTrackerCollection::NotifyFrameEnd(
    const viz::BeginFrameArgs& args) {
  for (auto& tracker : frame_trackers_) {
    tracker.second->ReportFrameEnd(args);
  }
}

void FrameSequenceTrackerCollection::NotifyFramePresented(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportFramePresented(frame_token, feedback);

  for (auto& tracker : removal_trackers_)
    tracker->ReportFramePresented(frame_token, feedback);

  for (auto& tracker : removal_trackers_) {
    if (tracker->termination_status() ==
        FrameSequenceTracker::TerminationStatus::kReadyForTermination) {
      // The tracker is ready to be terminated. Take the metrics from the
      // tracker, merge with any outstanding metrics from previous trackers of
      // the same type. If there are enough frames to report the metrics, then
      // report the metrics and destroy it. Otherwise, retain it to be merged
      // with follow-up sequences.
      auto metrics = tracker->TakeMetrics();
      if (accumulated_metrics_.contains(tracker->type())) {
        metrics->Merge(std::move(accumulated_metrics_[tracker->type()]));
        accumulated_metrics_.erase(tracker->type());
      }
      if (metrics->HasEnoughDataForReporting())
        metrics->ReportMetrics();
      if (metrics->HasDataLeftForReporting())
        accumulated_metrics_[tracker->type()] = std::move(metrics);
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

FrameSequenceTracker* FrameSequenceTrackerCollection::GetTrackerForTesting(
    FrameSequenceTrackerType type) {
  if (!frame_trackers_.contains(type))
    return nullptr;
  return frame_trackers_[type].get();
}

void FrameSequenceTrackerCollection::SetUkmManager(UkmManager* manager) {
  DCHECK(frame_trackers_.empty());
  ukm_manager_ = manager;
}

////////////////////////////////////////////////////////////////////////////////
// FrameSequenceTracker

FrameSequenceTracker::FrameSequenceTracker(
    FrameSequenceTrackerType type,
    UkmManager* manager,
    ThroughputUkmReporter* throughput_ukm_reporter)
    : type_(type),
      metrics_(
          std::make_unique<FrameSequenceMetrics>(type,
                                                 manager,
                                                 throughput_ukm_reporter)) {
  DCHECK_LT(type_, FrameSequenceTrackerType::kMaxType);
}

FrameSequenceTracker::~FrameSequenceTracker() {
}

void FrameSequenceTracker::ReportMetricsForTesting() {
  metrics_->ReportMetrics();
}

void FrameSequenceTracker::ReportBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

#if DCHECK_IS_ON()
  DCHECK(!is_inside_frame_) << TRACKER_DCHECK_MSG;
  is_inside_frame_ = true;
  last_started_impl_sequence_ = args.frame_id.sequence_number;

  if (args.type == viz::BeginFrameArgs::NORMAL)
    impl_frames_.insert(args.frame_id);
#endif
  TRACKER_TRACE_STREAM << "b(" << args.frame_id.sequence_number << ")";
  UpdateTrackedFrameData(&begin_impl_frame_data_, args.frame_id.source_id,
                         args.frame_id.sequence_number);
  impl_throughput().frames_expected +=
      begin_impl_frame_data_.previous_sequence_delta;

  if (first_frame_timestamp_.is_null())
    first_frame_timestamp_ = args.frame_time;
}

void FrameSequenceTracker::ReportBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  if (ShouldIgnoreSequence(args.frame_id.sequence_number))
    return;

#if DCHECK_IS_ON()
  if (args.type == viz::BeginFrameArgs::NORMAL) {
    DCHECK(impl_frames_.contains(args.frame_id));
  }
#endif

  TRACKER_TRACE_STREAM << 'B';
  TRACKER_TRACE_STREAM << "(" << begin_main_frame_data_.previous_sequence << ","
                       << args.frame_id.sequence_number << ")";
  UpdateTrackedFrameData(&begin_main_frame_data_, args.frame_id.source_id,
                         args.frame_id.sequence_number);
  if (!first_received_main_sequence_ ||
      first_received_main_sequence_ <= last_no_main_damage_sequence_)
    first_received_main_sequence_ = args.frame_id.sequence_number;
  main_throughput().frames_expected +=
      begin_main_frame_data_.previous_sequence_delta;
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
  last_processed_impl_sequence_ = ack.frame_id.sequence_number;
#endif
  if (first_submitted_frame_ == 0)
    first_submitted_frame_ = frame_token;
  last_submitted_frame_ = frame_token;

  TRACKER_TRACE_STREAM << 's';
  const bool main_changes_after_sequence_started =
      first_received_main_sequence_ &&
      origin_args.frame_id.sequence_number >= first_received_main_sequence_;
  const bool main_changes_include_new_changes =
      last_submitted_main_sequence_ == 0 ||
      origin_args.frame_id.sequence_number > last_submitted_main_sequence_;
  const bool main_change_had_no_damage =
      last_no_main_damage_sequence_ != 0 &&
      origin_args.frame_id.sequence_number == last_no_main_damage_sequence_;

  if (!ShouldIgnoreBeginFrameSource(origin_args.frame_id.source_id) &&
      main_changes_after_sequence_started && main_changes_include_new_changes &&
      !main_change_had_no_damage) {
    TRACKER_TRACE_STREAM << "S(" << origin_args.frame_id.sequence_number << ")";

    last_submitted_main_sequence_ = origin_args.frame_id.sequence_number;
    main_frames_.push_back(frame_token);
    DCHECK_GE(main_throughput().frames_expected, main_frames_.size())
        << TRACKER_DCHECK_MSG;
  }

  if (has_missing_content) {
    checkerboarding_.frames.push_back(frame_token);
  }
}

void FrameSequenceTracker::ReportFrameEnd(const viz::BeginFrameArgs& args) {
#if DCHECK_IS_ON()
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  TRACKER_TRACE_STREAM << "e(" << args.frame_id.sequence_number << ")";
  if (ShouldIgnoreSequence(args.frame_id.sequence_number)) {
    is_inside_frame_ = false;
    return;
  }

  DCHECK(is_inside_frame_) << TRACKER_DCHECK_MSG;
  DCHECK_EQ(last_started_impl_sequence_, last_processed_impl_sequence_)
      << TRACKER_DCHECK_MSG;
  is_inside_frame_ = false;
#endif
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

  if (ignored_frame_tokens_.contains(frame_token))
    return;

  TRACKER_TRACE_STREAM << 'P';

  TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(
      "cc,benchmark", "FrameSequenceTracker", metrics_.get(), "FramePresented",
      feedback.timestamp);
  const bool was_presented = !feedback.timestamp.is_null();
  if (was_presented && last_submitted_frame_) {
    DCHECK_LT(impl_throughput().frames_produced,
              impl_throughput().frames_expected)
        << TRACKER_DCHECK_MSG;
    ++impl_throughput().frames_produced;

    if (frame_token_acks_last_frame)
      last_submitted_frame_ = 0;
  }

  while (!main_frames_.empty() &&
         !viz::FrameTokenGT(main_frames_.front(), frame_token)) {
    if (was_presented && main_frames_.front() == frame_token) {
      DCHECK_LT(main_throughput().frames_produced,
                main_throughput().frames_expected)
          << TRACKER_DCHECK_MSG;
      ++main_throughput().frames_produced;
    }
    main_frames_.pop_front();
  }

  if (was_presented) {
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

  // It is possible that this is called before a begin-impl-frame has been
  // dispatched for this frame-sequence. In such cases, ignore this call.
  if (ShouldIgnoreSequence(ack.frame_id.sequence_number))
    return;
#if DCHECK_IS_ON()
  last_processed_impl_sequence_ = ack.frame_id.sequence_number;
#endif

  TRACKER_TRACE_STREAM << 'n';
  DCHECK_GT(impl_throughput().frames_expected, 0u) << TRACKER_DCHECK_MSG;
  DCHECK_GT(impl_throughput().frames_expected,
            impl_throughput().frames_produced)
      << TRACKER_DCHECK_MSG;
  --impl_throughput().frames_expected;

  if (begin_impl_frame_data_.previous_sequence == ack.frame_id.sequence_number)
    begin_impl_frame_data_.previous_sequence = 0;
}

void FrameSequenceTracker::ReportMainFrameCausedNoDamage(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.frame_id.source_id))
    return;

  if (ShouldIgnoreSequence(args.frame_id.sequence_number))
    return;

  TRACKER_TRACE_STREAM << 'N';
  TRACKER_TRACE_STREAM << "(" << begin_main_frame_data_.previous_sequence << ","
                       << args.frame_id.sequence_number << ")";
  DCHECK_GT(main_throughput().frames_expected, 0u) << TRACKER_DCHECK_MSG;
  DCHECK_GT(main_throughput().frames_expected,
            main_throughput().frames_produced)
      << TRACKER_DCHECK_MSG;
  last_no_main_damage_sequence_ = args.frame_id.sequence_number;
  --main_throughput().frames_expected;
  DCHECK_GE(main_throughput().frames_expected, main_frames_.size())
      << TRACKER_DCHECK_MSG;

  if (begin_main_frame_data_.previous_sequence == args.frame_id.sequence_number)
    begin_main_frame_data_.previous_sequence = 0;
}

void FrameSequenceTracker::PauseFrameProduction() {
  // Reset the states, so that the tracker ignores the vsyncs until the next
  // received begin-frame.
  begin_impl_frame_data_ = {0, 0, 0};
  begin_main_frame_data_ = {0, 0, 0};
  TRACKER_TRACE_STREAM << 'R';
}

void FrameSequenceTracker::UpdateTrackedFrameData(TrackedFrameData* frame_data,
                                                  uint64_t source_id,
                                                  uint64_t sequence_number) {
  if (frame_data->previous_sequence &&
      frame_data->previous_source == source_id) {
    uint32_t current_latency = sequence_number - frame_data->previous_sequence;
    DCHECK_GT(current_latency, 0u);
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
    FrameSequenceTrackerType sequence_type,
    ThreadType thread_type,
    int metric_index,
    const ThroughputData& data) {
  DCHECK_LT(sequence_type, FrameSequenceTrackerType::kMaxType);

  STATIC_HISTOGRAM_POINTER_GROUP(
      GetFrameSequenceLengthHistogramName(sequence_type), sequence_type,
      FrameSequenceTrackerType::kMaxType, Add(data.frames_expected),
      base::Histogram::FactoryGet(
          GetFrameSequenceLengthHistogramName(sequence_type), 1, 1000, 50,
          base::HistogramBase::kUmaTargetedHistogramFlag));

  if (data.frames_expected < kMinFramesForThroughputMetric)
    return base::nullopt;

  const int percent =
      static_cast<int>(100 * data.frames_produced / data.frames_expected);

  const bool is_animation =
      ShouldReportForAnimation(sequence_type, thread_type);
  const bool is_interaction =
      ShouldReportForInteraction(sequence_type, thread_type);

  if (is_animation) {
    UMA_HISTOGRAM_PERCENTAGE("Graphics.Smoothness.Throughput.AllAnimations",
                             percent);
  }

  if (is_interaction) {
    UMA_HISTOGRAM_PERCENTAGE("Graphics.Smoothness.Throughput.AllInteractions",
                             percent);
  }

  if (is_animation || is_interaction) {
    UMA_HISTOGRAM_PERCENTAGE("Graphics.Smoothness.Throughput.AllSequences",
                             percent);
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

}  // namespace cc
