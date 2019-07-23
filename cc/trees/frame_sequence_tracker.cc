// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/frame_sequence_tracker.h"

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc {

namespace {

enum class ThreadType {
  kMain,
  kCompositor,
};

constexpr const char* const kBuiltinSequences[] = {
    [FrameSequenceTrackerType::kCompositorAnimation] = "CompositorAnimation",
    [FrameSequenceTrackerType::kMainThreadAnimation] = "MainThreadAnimation",
    [FrameSequenceTrackerType::kPinchZoom] = "PinchZoom",
    [FrameSequenceTrackerType::kRAF] = "RAF",
    [FrameSequenceTrackerType::kTouchScroll] = "TouchScroll",
    [FrameSequenceTrackerType::kWheelScroll] = "WheelScroll",
};

constexpr int kBuiltinSequenceNum = base::size(kBuiltinSequences);
constexpr int kMaximumHistogramIndex = 2 * kBuiltinSequenceNum;

int GetIndexForMetric(ThreadType thread_type, FrameSequenceTrackerType type) {
  return thread_type == ThreadType::kMain
             ? static_cast<int>(type)
             : static_cast<int>(type + kBuiltinSequenceNum);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FrameSequenceTrackerCollection

FrameSequenceTrackerCollection::FrameSequenceTrackerCollection() {}
FrameSequenceTrackerCollection::~FrameSequenceTrackerCollection() {
  removal_trackers_.clear();
  DCHECK(frame_trackers_.empty());
}

std::unique_ptr<FrameSequenceTracker>
FrameSequenceTrackerCollection::CreateTracker(FrameSequenceTrackerType type) {
  // The collection always outlives the trackers. So using Unretained() here is
  // safe.
  auto tracker = base::WrapUnique(new FrameSequenceTracker(
      type, base::BindOnce(&FrameSequenceTrackerCollection::RemoveFrameTracker,
                           base::Unretained(this))));
  AddFrameTracker(tracker.get());
  return tracker;
}

void FrameSequenceTrackerCollection::ScheduleRemoval(
    std::unique_ptr<FrameSequenceTracker> tracker) {
  if (!tracker)
    return;
  tracker->ScheduleTerminate();
  removal_trackers_.push_back(std::move(tracker));
}

void FrameSequenceTrackerCollection::ClearAll() {
  removal_trackers_.clear();
  DCHECK(frame_trackers_.empty());
}

void FrameSequenceTrackerCollection::NotifyBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  for (auto* tracker : frame_trackers_) {
    tracker->ReportBeginImplFrame(args);
  }
}

void FrameSequenceTrackerCollection::NotifyBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  for (auto* tracker : frame_trackers_) {
    tracker->ReportBeginMainFrame(args);
  }
}

void FrameSequenceTrackerCollection::NotifyImplFrameCausedNoDamage(
    const viz::BeginFrameAck& ack) {
  for (auto* tracker : frame_trackers_) {
    tracker->ReportImplFrameCausedNoDamage(ack);
  }
}

void FrameSequenceTrackerCollection::NotifyMainFrameCausedNoDamage(
    const viz::BeginFrameArgs& args) {
  for (auto* tracker : frame_trackers_) {
    tracker->ReportMainFrameCausedNoDamage(args);
  }
}

void FrameSequenceTrackerCollection::NotifyPauseFrameProduction() {
  for (auto* tracker : frame_trackers_)
    tracker->PauseFrameProduction();
}

void FrameSequenceTrackerCollection::NotifySubmitFrame(
    uint32_t frame_token,
    const viz::BeginFrameAck& ack,
    const viz::BeginFrameArgs& origin_args) {
  for (auto* tracker : frame_trackers_) {
    tracker->ReportSubmitFrame(frame_token, ack, origin_args);
  }
}

void FrameSequenceTrackerCollection::NotifyFramePresented(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  for (auto* tracker : frame_trackers_)
    tracker->ReportFramePresented(frame_token, feedback);

  // Destroy the trackers that are ready to be terminated.
  base::EraseIf(
      removal_trackers_,
      [](const std::unique_ptr<FrameSequenceTracker>& tracker) {
        return tracker->termination_status() ==
               FrameSequenceTracker::TerminationStatus::kReadyForTermination;
      });
}

void FrameSequenceTrackerCollection::AddFrameTracker(
    FrameSequenceTracker* tracker) {
  frame_trackers_.push_back(tracker);
}

void FrameSequenceTrackerCollection::RemoveFrameTracker(
    FrameSequenceTracker* tracker) {
  base::Erase(frame_trackers_, tracker);
}

////////////////////////////////////////////////////////////////////////////////
// FrameSequenceTracker

FrameSequenceTracker::FrameSequenceTracker(
    FrameSequenceTrackerType type,
    base::OnceCallback<void(FrameSequenceTracker*)> destroy_callback)
    : type_(type), destroy_callback_(std::move(destroy_callback)) {
  DCHECK_LT(type_, FrameSequenceTrackerType::kMaxType);
  TRACE_EVENT_ASYNC_BEGIN1("cc,benchmark", "FrameSequenceTracker", this, "name",
                           TRACE_STR_COPY(kBuiltinSequences[type_]));
}

FrameSequenceTracker::~FrameSequenceTracker() {
  DCHECK_LE(impl_throughput_.frames_produced, impl_throughput_.frames_expected);
  DCHECK_LE(main_throughput_.frames_produced, main_throughput_.frames_expected);
  DCHECK_LE(main_throughput_.frames_produced, impl_throughput_.frames_produced);
  TRACE_EVENT_ASYNC_END1(
      "cc,benchmark", "FrameSequenceTracker", this, "args",
      ThroughputData::ToTracedValue(impl_throughput_, main_throughput_));
  ThroughputData::ReportHistogram(
      type_, "CompositorThread",
      GetIndexForMetric(ThreadType::kCompositor, type_), impl_throughput_);
  ThroughputData::ReportHistogram(type_, "MainThread",
                                  GetIndexForMetric(ThreadType::kMain, type_),
                                  main_throughput_);
  std::move(destroy_callback_).Run(this);
}

void FrameSequenceTracker::ReportBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.source_id))
    return;

  UpdateTrackedFrameData(&begin_impl_frame_data_, args.source_id,
                         args.sequence_number);
  impl_throughput_.frames_expected +=
      begin_impl_frame_data_.previous_sequence_delta;
}

void FrameSequenceTracker::ReportBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.source_id))
    return;

  UpdateTrackedFrameData(&begin_main_frame_data_, args.source_id,
                         args.sequence_number);
  if (first_received_main_sequence_ == 0)
    first_received_main_sequence_ = args.sequence_number;
  main_throughput_.frames_expected +=
      begin_main_frame_data_.previous_sequence_delta;
}

void FrameSequenceTracker::ReportSubmitFrame(
    uint32_t frame_token,
    const viz::BeginFrameAck& ack,
    const viz::BeginFrameArgs& origin_args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(ack.source_id))
    return;

  if (begin_impl_frame_data_.previous_sequence == 0 ||
      ack.sequence_number < begin_impl_frame_data_.previous_sequence) {
    return;
  }

  if (first_submitted_frame_ == 0)
    first_submitted_frame_ = frame_token;
  last_submitted_frame_ = frame_token;

  if (!ShouldIgnoreBeginFrameSource(origin_args.source_id) &&
      first_received_main_sequence_ &&
      origin_args.sequence_number >= first_received_main_sequence_) {
    if (last_submitted_main_sequence_ == 0 ||
        origin_args.sequence_number > last_submitted_main_sequence_) {
      last_submitted_main_sequence_ = origin_args.sequence_number;
      main_frames_.push_back(frame_token);
      DCHECK_GE(main_throughput_.frames_expected, main_frames_.size());
    }
  }
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

  const bool was_presented = !feedback.timestamp.is_null();
  if (was_presented && last_submitted_frame_) {
    DCHECK_LT(impl_throughput_.frames_produced,
              impl_throughput_.frames_expected);
    ++impl_throughput_.frames_produced;

    if (frame_token_acks_last_frame)
      last_submitted_frame_ = 0;
  }

  while (!main_frames_.empty() &&
         !viz::FrameTokenGT(main_frames_.front(), frame_token)) {
    if (was_presented && main_frames_.front() == frame_token) {
      DCHECK_LT(main_throughput_.frames_produced,
                main_throughput_.frames_expected);
      ++main_throughput_.frames_produced;
    }
    main_frames_.pop_front();
  }
}

void FrameSequenceTracker::ReportImplFrameCausedNoDamage(
    const viz::BeginFrameAck& ack) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(ack.source_id))
    return;

  // It is possible that this is called before a begin-impl-frame has been
  // dispatched for this frame-sequence. In such cases, ignore this call.
  if (begin_impl_frame_data_.previous_sequence == 0 ||
      ack.sequence_number < begin_impl_frame_data_.previous_sequence) {
    return;
  }
  DCHECK_GT(impl_throughput_.frames_expected, 0u);
  DCHECK_GT(impl_throughput_.frames_expected, impl_throughput_.frames_produced);
  --impl_throughput_.frames_expected;

  if (begin_impl_frame_data_.previous_sequence == ack.sequence_number)
    begin_impl_frame_data_.previous_sequence = 0;
}

void FrameSequenceTracker::ReportMainFrameCausedNoDamage(
    const viz::BeginFrameArgs& args) {
  if (termination_status_ != TerminationStatus::kActive)
    return;

  if (ShouldIgnoreBeginFrameSource(args.source_id))
    return;

  // It is possible that this is called before a begin-main-frame has been
  // dispatched for this frame-sequence. In such cases, ignore this call.
  if (begin_main_frame_data_.previous_sequence == 0 ||
      args.sequence_number < begin_main_frame_data_.previous_sequence) {
    return;
  }

  DCHECK_GT(main_throughput_.frames_expected, 0u);
  DCHECK_GT(main_throughput_.frames_expected, main_throughput_.frames_produced);
  --main_throughput_.frames_expected;
  DCHECK_GE(main_throughput_.frames_expected, main_frames_.size());

  if (begin_main_frame_data_.previous_sequence == args.sequence_number)
    begin_main_frame_data_.previous_sequence = 0;
}

void FrameSequenceTracker::PauseFrameProduction() {
  // Reset the states, so that the tracker ignores the vsyncs until the next
  // received begin-frame.
  begin_impl_frame_data_ = {0, 0, 0};
  begin_main_frame_data_ = {0, 0, 0};
}

void FrameSequenceTracker::UpdateTrackedFrameData(TrackedFrameData* frame_data,
                                                  uint64_t source_id,
                                                  uint64_t sequence_number) {
  if (frame_data->previous_sequence &&
      frame_data->previous_source == source_id) {
    uint8_t current_latency = sequence_number - frame_data->previous_sequence;
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
    return false;
  return source_id != begin_impl_frame_data_.previous_source;
}

std::unique_ptr<base::trace_event::TracedValue>
FrameSequenceTracker::ThroughputData::ToTracedValue(
    const ThroughputData& impl,
    const ThroughputData& main) {
  auto dict = std::make_unique<base::trace_event::TracedValue>();
  dict->SetInteger("impl-frames-produced", impl.frames_produced);
  dict->SetInteger("impl-frames-expected", impl.frames_expected);
  dict->SetInteger("main-frames-produced", main.frames_produced);
  dict->SetInteger("main-frames-expected", main.frames_expected);
  return dict;
}

void FrameSequenceTracker::ThroughputData::ReportHistogram(
    FrameSequenceTrackerType sequence_type,
    const char* thread_name,
    int metric_index,
    const ThroughputData& data) {
  DCHECK_LT(sequence_type, FrameSequenceTrackerType::kMaxType);

  UMA_HISTOGRAM_COUNTS_1000("Graphics.Smoothness.FrameSequenceLength",
                            data.frames_expected);

  // Avoid reporting any throughput metric for sequences that had a small amount
  // of frames.
  constexpr int kMinFramesForThroughputMetric = 4;
  if (data.frames_expected < kMinFramesForThroughputMetric)
    return;

  const std::string name =
      base::StrCat({"Graphics.Smoothness.Throughput.", thread_name, ".",
                    kBuiltinSequences[sequence_type]});
  const int percent =
      static_cast<int>(100 * data.frames_produced / data.frames_expected);
  STATIC_HISTOGRAM_POINTER_GROUP(
      name, metric_index, kMaximumHistogramIndex, Add(percent),
      base::LinearHistogram::FactoryGet(
          name, 1, 100, 101, base::HistogramBase::kUmaTargetedHistogramFlag));
}

}  // namespace cc
