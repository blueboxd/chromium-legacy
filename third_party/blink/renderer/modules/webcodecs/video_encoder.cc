// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_metadata.h"
#include "third_party/blink/renderer/modules/webcodecs/video_encoder_encode_options.h"
#include "third_party/blink/renderer/modules/webcodecs/video_encoder_init.h"
#include "third_party/blink/renderer/modules/webcodecs/video_encoder_tune_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

ScriptPromise RejectedPromise(ScriptState* script_state,
                              DOMExceptionCode code,
                              const String& message) {
  return ScriptPromise::RejectWithDOMException(
      script_state, MakeGarbageCollected<DOMException>(code, message));
}

}  // namespace

// static
VideoEncoder* VideoEncoder::Create(ScriptState* script_state,
                                   VideoEncoderInit* init,
                                   ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<VideoEncoder>(script_state, init, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return result;
}

VideoEncoder::VideoEncoder(ScriptState* script_state,
                           VideoEncoderInit* init,
                           ExceptionState& exception_state)
    : script_state_(script_state) {
  if (init->codec() != "NoOpCodec") {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "Codec not found.");
    return;
  }
  auto* tune_options = init->tuneOptions();
  if (!tune_options) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "tuneOptions is not populated");
    return;
  }

  if (!tune_options->hasHeight() || tune_options->height() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "Invalid height.");
    return;
  }

  if (!tune_options->hasWidth() || tune_options->width() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "Invalid width.");
    return;
  }

  if (!init->hasOutput()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "output_callback was not provided");
    return;
  }

  output_callback_ = init->output();
  error_callback_ = init->error();
  frame_size_ = gfx::Size(tune_options->width(), tune_options->height());
}

VideoEncoder::~VideoEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ScriptPromise VideoEncoder::tune(const VideoEncoderTuneOptions* params,
                                 ExceptionState&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return RejectedPromise(script_state_, DOMExceptionCode::kNotSupportedError,
                         "tune() is not implemented yet");
}

ScriptPromise VideoEncoder::encode(const VideoFrame* frame,
                                   const VideoEncoderEncodeOptions* params,
                                   ExceptionState&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!output_callback_) {
    return RejectedPromise(script_state_, DOMExceptionCode::kInvalidStateError,
                           "VideoEncoder hasn't been initialized");
  }
  if (frame->codedWidth() != static_cast<uint32_t>(frame_size_.width()) ||
      frame->codedHeight() != static_cast<uint32_t>(frame_size_.height())) {
    return RejectedPromise(
        script_state_, DOMExceptionCode::kConstraintError,
        "Frame size doesn't match initial encoder parameters.");
  }

  bool keyframe = params->hasKeyFrame() && params->keyFrame();
  DoEncoding(frame, keyframe);
  return ScriptPromise::CastUndefined(script_state_);
}

ScriptPromise VideoEncoder::close() {
  return ScriptPromise::CastUndefined(script_state_);
}

void VideoEncoder::CallOutputCallback(EncodedVideoChunk* chunk) {
  if (!script_state_->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk);
}

void VideoEncoder::DoEncoding(const VideoFrame* frame, bool force_keyframe) {
  SEQUENCE_CHECKER(sequence_checker_);
  auto media_frame = frame->frame();

  EncodedVideoMetadata metadata;
  metadata.timestamp = media_frame->timestamp();
  metadata.key_frame = force_keyframe;
  auto duration = frame->duration();
  if (duration.has_value())
    metadata.duration = base::TimeDelta::FromMicroseconds(duration.value());

  // TODO(crbug/1045248): Here is the place where actual video encoder is
  // going to be called.
  // Currently we just take data from the Y plane and pretend that it's
  // an encoded video chunk.

  auto* data =
      DOMArrayBuffer::Create(media_frame->data(media::VideoFrame::kYPlane),
                             media_frame->stride(media::VideoFrame::kYPlane));

  auto* chunk = MakeGarbageCollected<EncodedVideoChunk>(metadata, data);
  CallOutputCallback(chunk);
}

void VideoEncoder::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(output_callback_);
  visitor->Trace(error_callback_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
