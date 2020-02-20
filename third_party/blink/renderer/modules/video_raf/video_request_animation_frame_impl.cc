// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/video_raf/video_request_animation_frame_impl.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_metadata.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/video_raf/video_frame_request_callback_collection.h"
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

void VideoRequestAnimationFrameImpl::OnRequestAnimationFrame(
    base::TimeTicks presentation_time,
    base::TimeTicks expected_presentation_time,
    uint32_t presented_frames_counter,
    const media::VideoFrame& presented_frame) {
  DCHECK(RuntimeEnabledFeatures::VideoRequestAnimationFrameEnabled());

  // Skip this work if there are no registered callbacks.
  if (callback_collection_->IsEmpty())
    return;

  auto& time_converter =
      GetSupplementable()->GetDocument().Loader()->GetTiming();
  metadata_ = VideoFrameMetadata::Create();

  metadata_->setPresentationTime(
      time_converter.MonotonicTimeToZeroBasedDocumentTime(presentation_time)
          .InMillisecondsF());

  metadata_->setExpectedPresentationTime(
      time_converter
          .MonotonicTimeToZeroBasedDocumentTime(expected_presentation_time)
          .InMillisecondsF());

  metadata_->setWidth(presented_frame.visible_rect().width());
  metadata_->setHeight(presented_frame.visible_rect().height());

  metadata_->setPresentationTimestamp(presented_frame.timestamp().InSecondsF());

  base::TimeDelta elapsed;
  if (presented_frame.metadata()->GetTimeDelta(
          media::VideoFrameMetadata::PROCESSING_TIME, &elapsed)) {
    metadata_->setElapsedProcessingTime(elapsed.InSecondsF());
  }

  metadata_->setPresentedFrames(presented_frames_counter);

  base::TimeTicks time;
  if (presented_frame.metadata()->GetTimeTicks(
          media::VideoFrameMetadata::CAPTURE_BEGIN_TIME, &time)) {
    metadata_->setCaptureTime(
        time_converter.MonotonicTimeToZeroBasedDocumentTime(time)
            .InMillisecondsF());
  }

  double rtp_timestamp;
  if (presented_frame.metadata()->GetDouble(
          media::VideoFrameMetadata::RTP_TIMESTAMP, &rtp_timestamp)) {
    base::CheckedNumeric<uint32_t> uint_rtp_timestamp = rtp_timestamp;
    if (uint_rtp_timestamp.IsValid())
      metadata_->setRtpTimestamp(rtp_timestamp);
  }

  // If new video.rAF callbacks are queued before the pending ones complete,
  // we could end up here while there is still an outstanding call to
  // ExecuteFrameCallbacks(). Overriding |metadata_| is fine, as we will provide
  // the newest frame info to all callbacks (although it will look like we
  // missed a frame). However, we should not schedule a second call to
  // ExecuteFrameCallbacks(), as it could lead to some problematic results.
  //
  // TODO(https://crbug.com/1049761): Pull the video frame metadata in
  // ExecuteFrameCallbacks() instead.
  if (!pending_execution_) {
    pending_execution_ = true;
    GetSupplementable()
        ->GetDocument()
        .GetScriptedAnimationController()
        .ScheduleVideoRafExecution(
            WTF::Bind(&VideoRequestAnimationFrameImpl::ExecuteFrameCallbacks,
                      WrapWeakPersistent(this)));
  }
}

void VideoRequestAnimationFrameImpl::ExecuteFrameCallbacks(
    double high_res_now_ms) {
  DCHECK(metadata_);
  DCHECK(pending_execution_);
  callback_collection_->ExecuteFrameCallbacks(high_res_now_ms, metadata_);
  pending_execution_ = false;
  metadata_.Clear();
}

int VideoRequestAnimationFrameImpl::requestAnimationFrame(
    V8VideoFrameRequestCallback* callback) {
  if (auto* player = GetSupplementable()->GetWebMediaPlayer())
    player->RequestAnimationFrame();

  auto* frame_callback = MakeGarbageCollected<
      VideoFrameRequestCallbackCollection::V8VideoFrameCallback>(callback);

  return callback_collection_->RegisterFrameCallback(frame_callback);
}

void VideoRequestAnimationFrameImpl::cancelAnimationFrame(int id) {
  callback_collection_->CancelFrameCallback(id);
}

void VideoRequestAnimationFrameImpl::Trace(Visitor* visitor) {
  visitor->Trace(metadata_);
  visitor->Trace(callback_collection_);
  Supplement<HTMLVideoElement>::Trace(visitor);
}

}  // namespace blink
