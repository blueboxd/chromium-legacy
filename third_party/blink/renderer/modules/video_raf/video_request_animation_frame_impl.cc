// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/video_raf/video_request_animation_frame_impl.h"

#include <memory>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_metadata.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/time_clamper.h"
#include "third_party/blink/renderer/modules/video_raf/video_frame_request_callback_collection.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
// Returns whether or not a video's frame rate is close to the browser's frame
// rate, as measured by their rendering intervals. For example, on a 60hz
// screen, this should return false for a 25fps video and true for a 60fps
// video. On a 144hz screen, both videos would return false.
static bool IsFrameRateRelativelyHigh(base::TimeDelta rendering_interval,
                                      base::TimeDelta average_frame_duration) {
  if (average_frame_duration.is_zero())
    return false;

  constexpr double kThreshold = 0.05;
  return kThreshold >
         std::abs(1.0 - (rendering_interval.InMillisecondsF() /
                         average_frame_duration.InMillisecondsF()));
}

}  // namespace

VideoRequestAnimationFrameImpl::VideoRequestAnimationFrameImpl(
    HTMLVideoElement& element)
    : VideoRequestAnimationFrame(element),
      callback_collection_(
          MakeGarbageCollected<VideoFrameRequestCallbackCollection>(
              element.GetExecutionContext())) {}

// static
VideoRequestAnimationFrameImpl& VideoRequestAnimationFrameImpl::From(
    HTMLVideoElement& element) {
  VideoRequestAnimationFrameImpl* supplement =
      Supplement<HTMLVideoElement>::From<VideoRequestAnimationFrameImpl>(
          element);
  if (!supplement) {
    supplement = MakeGarbageCollected<VideoRequestAnimationFrameImpl>(element);
    Supplement<HTMLVideoElement>::ProvideTo(element, supplement);
  }

  return *supplement;
}

// static
int VideoRequestAnimationFrameImpl::requestAnimationFrame(
    HTMLVideoElement& element,
    V8VideoFrameRequestCallback* callback) {
  return VideoRequestAnimationFrameImpl::From(element).requestAnimationFrame(
      callback);
}

// static
void VideoRequestAnimationFrameImpl::cancelAnimationFrame(
    HTMLVideoElement& element,
    int callback_id) {
  VideoRequestAnimationFrameImpl::From(element).cancelAnimationFrame(
      callback_id);
}

void VideoRequestAnimationFrameImpl::OnWebMediaPlayerCreated() {
  DCHECK(RuntimeEnabledFeatures::VideoRequestAnimationFrameEnabled());
  if (!callback_collection_->IsEmpty())
    GetSupplementable()->GetWebMediaPlayer()->RequestAnimationFrame();
}

void VideoRequestAnimationFrameImpl::ScheduleCallbackExecution() {
  TRACE_EVENT1("blink",
               "VideoRequestAnimationFrameImpl::ScheduleCallbackExecution",
               "did_schedule", !pending_execution_);

  if (pending_execution_)
    return;

  pending_execution_ = true;
  GetSupplementable()
      ->GetDocument()
      .GetScriptedAnimationController()
      .ScheduleVideoRafExecution(
          WTF::Bind(&VideoRequestAnimationFrameImpl::OnRenderingSteps,
                    WrapWeakPersistent(this)));
}

void VideoRequestAnimationFrameImpl::OnRequestAnimationFrame() {
  DCHECK(RuntimeEnabledFeatures::VideoRequestAnimationFrameEnabled());
  TRACE_EVENT1("blink",
               "VideoRequestAnimationFrameImpl::OnRequestAnimationFrame",
               "has_callbacks", !callback_collection_->IsEmpty());

  // Skip this work if there are no registered callbacks.
  if (callback_collection_->IsEmpty())
    return;

  ScheduleCallbackExecution();
}

void VideoRequestAnimationFrameImpl::ExecuteFrameCallbacks(
    double high_res_now_ms,
    std::unique_ptr<WebMediaPlayer::VideoFramePresentationMetadata>
        frame_metadata) {
  TRACE_EVENT0("blink",
               "VideoRequestAnimationFrameImpl::ExecuteFrameCallbacks");

  last_presented_frames_ = frame_metadata->presented_frames;

  auto* metadata = VideoFrameMetadata::Create();
  auto& time_converter =
      GetSupplementable()->GetDocument().Loader()->GetTiming();

  metadata->setPresentationTime(GetClampedTimeInMillis(
      time_converter.MonotonicTimeToZeroBasedDocumentTime(
          frame_metadata->presentation_time)));

  metadata->setExpectedDisplayTime(GetClampedTimeInMillis(
      time_converter.MonotonicTimeToZeroBasedDocumentTime(
          frame_metadata->expected_display_time)));

  metadata->setPresentedFrames(frame_metadata->presented_frames);

  metadata->setWidth(frame_metadata->width);
  metadata->setHeight(frame_metadata->height);

  metadata->setMediaTime(frame_metadata->media_time.InSecondsF());

  base::TimeDelta processing_duration;
  if (frame_metadata->metadata.GetTimeDelta(
          media::VideoFrameMetadata::PROCESSING_TIME, &processing_duration)) {
    metadata->setProcessingDuration(
        GetCoarseClampedTimeInSeconds(processing_duration));
  }

  base::TimeTicks capture_time;
  if (frame_metadata->metadata.GetTimeTicks(
          media::VideoFrameMetadata::CAPTURE_BEGIN_TIME, &capture_time)) {
    metadata->setCaptureTime(GetClampedTimeInMillis(
        time_converter.MonotonicTimeToZeroBasedDocumentTime(capture_time)));
  }

  base::TimeTicks receive_time;
  if (frame_metadata->metadata.GetTimeTicks(
          media::VideoFrameMetadata::RECEIVE_TIME, &receive_time)) {
    metadata->setReceiveTime(GetClampedTimeInMillis(
        time_converter.MonotonicTimeToZeroBasedDocumentTime(receive_time)));
  }

  double rtp_timestamp;
  if (frame_metadata->metadata.GetDouble(
          media::VideoFrameMetadata::RTP_TIMESTAMP, &rtp_timestamp)) {
    base::CheckedNumeric<uint32_t> uint_rtp_timestamp = rtp_timestamp;
    if (uint_rtp_timestamp.IsValid())
      metadata->setRtpTimestamp(rtp_timestamp);
  }

  callback_collection_->ExecuteFrameCallbacks(high_res_now_ms, metadata);
}

void VideoRequestAnimationFrameImpl::OnRenderingSteps(double high_res_now_ms) {
  DCHECK(pending_execution_);
  TRACE_EVENT1("blink", "VideoRequestAnimationFrameImpl::OnRenderingSteps",
               "has_callbacks", !callback_collection_->IsEmpty());

  pending_execution_ = false;

  // Callbacks could have been canceled from the time we scheduled their
  // execution.
  if (callback_collection_->IsEmpty())
    return;

  auto* player = GetSupplementable()->GetWebMediaPlayer();
  if (!player)
    return;

  auto metadata = player->GetVideoFramePresentationMetadata();

  const bool is_hfr = IsFrameRateRelativelyHigh(
      metadata->rendering_interval, metadata->average_frame_duration);

  // Check if we have a new frame or not.
  if (last_presented_frames_ == metadata->presented_frames) {
    ++consecutive_stale_frames_;
  } else {
    consecutive_stale_frames_ = 0;
    ExecuteFrameCallbacks(high_res_now_ms, std::move(metadata));
  }

  // If the video's frame rate is relatively close to the screen's refresh rate
  // (or brower's current frame rate), schedule ourselves immediately.
  // Otherwise, jittering and thread hopping means that the call to
  // OnRequestAnimationFrame() would barely miss the rendering steps, and we
  // would miss a frame.
  // Also check |consecutive_stale_frames_| to make sure we don't schedule
  // executions when paused, or in other scenarios where potentially scheduling
  // extra rendering steps would be wasteful.
  if (is_hfr && !callback_collection_->IsEmpty() &&
      consecutive_stale_frames_ < 2) {
    ScheduleCallbackExecution();
  }
}

// static
double VideoRequestAnimationFrameImpl::GetClampedTimeInMillis(
    base::TimeDelta time) {
  constexpr double kSecondsToMillis = 1000.0;
  return Performance::ClampTimeResolution(time.InSecondsF()) * kSecondsToMillis;
}

// static
double VideoRequestAnimationFrameImpl::GetCoarseClampedTimeInSeconds(
    base::TimeDelta time) {
  constexpr double kCoarseResolutionInSeconds = 100e-6;
  // Add this assert, in case TimeClamper's resolution were to change to be
  // stricter.
  static_assert(kCoarseResolutionInSeconds >= TimeClamper::kResolutionSeconds,
                "kCoarseResolutionInSeconds should be at least "
                "as coarse as other clock resolutions");
  double interval = floor(time.InSecondsF() / kCoarseResolutionInSeconds);
  double clamped_time = interval * kCoarseResolutionInSeconds;

  return clamped_time;
}

int VideoRequestAnimationFrameImpl::requestAnimationFrame(
    V8VideoFrameRequestCallback* callback) {
  TRACE_EVENT0("blink",
               "VideoRequestAnimationFrameImpl::requestAnimationFrame");

  if (auto* player = GetSupplementable()->GetWebMediaPlayer())
    player->RequestAnimationFrame();

  auto* frame_callback = MakeGarbageCollected<
      VideoFrameRequestCallbackCollection::V8VideoFrameCallback>(callback);

  return callback_collection_->RegisterFrameCallback(frame_callback);
}

void VideoRequestAnimationFrameImpl::RegisterCallbackForTest(
    VideoFrameRequestCallbackCollection::VideoFrameCallback* callback) {
  pending_execution_ = true;

  callback_collection_->RegisterFrameCallback(callback);
}

void VideoRequestAnimationFrameImpl::cancelAnimationFrame(int id) {
  callback_collection_->CancelFrameCallback(id);
}

void VideoRequestAnimationFrameImpl::Trace(Visitor* visitor) {
  visitor->Trace(callback_collection_);
  Supplement<HTMLVideoElement>::Trace(visitor);
}

}  // namespace blink
