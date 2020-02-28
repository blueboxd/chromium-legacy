// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/video_raf/video_request_animation_frame_impl.h"

#include <memory>
#include <utility>

#include "media/base/media_switches.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_metadata.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/time_clamper.h"
#include "third_party/blink/renderer/modules/video_raf/video_frame_request_callback_collection.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

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
  if (callback_collection_->HasFrameCallback())
    GetSupplementable()->GetWebMediaPlayer()->RequestAnimationFrame();
}

void VideoRequestAnimationFrameImpl::OnRequestAnimationFrame() {
  DCHECK(RuntimeEnabledFeatures::VideoRequestAnimationFrameEnabled());

  // Skip this work if there are no registered callbacks.
  if (callback_collection_->IsEmpty())
    return;

  if (!pending_execution_) {
    pending_execution_ = true;
    if (base::FeatureList::IsEnabled(media::kUseMicrotaskForVideoRAF)) {
      auto& time_converter =
          GetSupplementable()->GetDocument().Loader()->GetTiming();
      Microtask::EnqueueMicrotask(WTF::Bind(
          &VideoRequestAnimationFrameImpl::ExecuteFrameCallbacks,
          WrapWeakPersistent(this),
          // TODO(crbug.com/1012063): Now is probably not the right value.
          time_converter
              .MonotonicTimeToZeroBasedDocumentTime(base::TimeTicks::Now())
              .InMillisecondsF()));
    } else {
      GetSupplementable()
          ->GetDocument()
          .GetScriptedAnimationController()
          .ScheduleVideoRafExecution(
              WTF::Bind(&VideoRequestAnimationFrameImpl::ExecuteFrameCallbacks,
                        WrapWeakPersistent(this)));
    }
  }
}

void VideoRequestAnimationFrameImpl::ExecuteFrameCallbacks(
    double high_res_now_ms) {
  DCHECK(pending_execution_);

  // Callbacks could have been canceled from the time we scheduled their
  // execution.
  if (callback_collection_->IsEmpty()) {
    pending_execution_ = false;
    return;
  }

  auto* player = GetSupplementable()->GetWebMediaPlayer();
  if (!player) {
    pending_execution_ = false;
    return;
  }

  auto frame_metadata = player->GetVideoFramePresentationMetadata();

  auto* metadata = VideoFrameMetadata::Create();
  auto& time_converter =
      GetSupplementable()->GetDocument().Loader()->GetTiming();

  metadata->setPresentationTime(GetClampedTimeInMillis(
      time_converter.MonotonicTimeToZeroBasedDocumentTime(
          frame_metadata->presentation_time)));

  metadata->setExpectedPresentationTime(GetClampedTimeInMillis(
      time_converter.MonotonicTimeToZeroBasedDocumentTime(
          frame_metadata->expected_presentation_time)));

  metadata->setPresentedFrames(frame_metadata->presented_frames);

  metadata->setWidth(frame_metadata->width);
  metadata->setHeight(frame_metadata->height);

  metadata->setPresentationTimestamp(
      frame_metadata->presentation_timestamp.InSecondsF());

  base::TimeDelta elapsed;
  if (frame_metadata->metadata.GetTimeDelta(
          media::VideoFrameMetadata::PROCESSING_TIME, &elapsed)) {
    metadata->setElapsedProcessingTime(GetCoarseClampedTimeInSeconds(elapsed));
  }

  base::TimeTicks capture_time;
  if (frame_metadata->metadata.GetTimeTicks(
          media::VideoFrameMetadata::CAPTURE_BEGIN_TIME, &capture_time)) {
    metadata->setCaptureTime(GetClampedTimeInMillis(
        time_converter.MonotonicTimeToZeroBasedDocumentTime(capture_time)));
  }

  double rtp_timestamp;
  if (frame_metadata->metadata.GetDouble(
          media::VideoFrameMetadata::RTP_TIMESTAMP, &rtp_timestamp)) {
    base::CheckedNumeric<uint32_t> uint_rtp_timestamp = rtp_timestamp;
    if (uint_rtp_timestamp.IsValid())
      metadata->setRtpTimestamp(rtp_timestamp);
  }

  callback_collection_->ExecuteFrameCallbacks(high_res_now_ms, metadata);
  pending_execution_ = false;
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
