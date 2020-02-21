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

void VideoRequestAnimationFrameImpl::OnRequestAnimationFrame() {
  DCHECK(RuntimeEnabledFeatures::VideoRequestAnimationFrameEnabled());

  // Skip this work if there are no registered callbacks.
  if (callback_collection_->IsEmpty())
    return;

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

  metadata->setPresentationTime(time_converter
                                    .MonotonicTimeToZeroBasedDocumentTime(
                                        frame_metadata->presentation_time)
                                    .InMillisecondsF());

  metadata->setExpectedPresentationTime(
      time_converter
          .MonotonicTimeToZeroBasedDocumentTime(
              frame_metadata->expected_presentation_time)
          .InMillisecondsF());

  metadata->setPresentedFrames(frame_metadata->presented_frames);

  metadata->setWidth(frame_metadata->width);
  metadata->setHeight(frame_metadata->height);

  metadata->setPresentationTimestamp(
      frame_metadata->presentation_timestamp.InSecondsF());

  base::TimeDelta elapsed;
  if (frame_metadata->metadata.GetTimeDelta(
          media::VideoFrameMetadata::PROCESSING_TIME, &elapsed)) {
    metadata->setElapsedProcessingTime(elapsed.InSecondsF());
  }

  base::TimeTicks time;
  if (frame_metadata->metadata.GetTimeTicks(
          media::VideoFrameMetadata::CAPTURE_BEGIN_TIME, &time)) {
    metadata->setCaptureTime(
        time_converter.MonotonicTimeToZeroBasedDocumentTime(time)
            .InMillisecondsF());
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
