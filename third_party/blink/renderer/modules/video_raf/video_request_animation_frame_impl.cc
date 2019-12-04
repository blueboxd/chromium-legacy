// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/video_raf/video_request_animation_frame_impl.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/video_raf/video_frame_metadata.h"
#include "third_party/blink/renderer/modules/video_raf/video_frame_request_callback_collection.h"

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
  auto* metadata = VideoFrameMetadata::Create();

  metadata->setPresentationTime(
      time_converter.MonotonicTimeToZeroBasedDocumentTime(presentation_time)
          .InMillisecondsF());

  metadata->setExpectedPresentationTime(
      time_converter
          .MonotonicTimeToZeroBasedDocumentTime(expected_presentation_time)
          .InMillisecondsF());

  metadata->setWidth(presented_frame.visible_rect().width());
  metadata->setHeight(presented_frame.visible_rect().height());

  metadata->setPresentationTimestamp(presented_frame.timestamp().InSecondsF());

  base::TimeDelta elapsed;
  if (presented_frame.metadata()->GetTimeDelta(
          media::VideoFrameMetadata::PROCESSING_TIME, &elapsed)) {
    metadata->setElapsedProcessingTime(elapsed.InSecondsF());
  }

  metadata->setPresentedFrames(presented_frames_counter);

  base::TimeTicks time;
  if (presented_frame.metadata()->GetTimeTicks(
          media::VideoFrameMetadata::CAPTURE_BEGIN_TIME, &time)) {
    metadata->setElapsedProcessingTime(
        time_converter.MonotonicTimeToZeroBasedDocumentTime(time)
            .InMillisecondsF());
  }

  double rtp_timestamp;
  if (presented_frame.metadata()->GetDouble(
          media::VideoFrameMetadata::RTP_TIMESTAMP, &rtp_timestamp)) {
    base::CheckedNumeric<uint32_t> uint_rtp_timestamp = rtp_timestamp;
    if (uint_rtp_timestamp.IsValid())
      metadata->setRtpTimestamp(rtp_timestamp);
  }

  callback_collection_->ExecuteFrameCallbacks(
      time_converter
          .MonotonicTimeToZeroBasedDocumentTime(base::TimeTicks::Now())
          .InMillisecondsF(),
      metadata);
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

void VideoRequestAnimationFrameImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(callback_collection_);
  Supplement<HTMLVideoElement>::Trace(visitor);
}

}  // namespace blink
