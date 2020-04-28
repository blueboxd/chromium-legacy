// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_encoder.h"
#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/video/vpx_video_encoder.h"
#endif
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_tune_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_metadata.h"
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
                                   ExceptionState& exception_state) {
  return MakeGarbageCollected<VideoEncoder>(script_state, exception_state);
}

VideoEncoder::VideoEncoder(ScriptState* script_state,
                           ExceptionState& exception_state)
    : script_state_(script_state) {}

VideoEncoder::~VideoEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ScriptPromise VideoEncoder::tune(const VideoEncoderTuneOptions* params,
                                 ExceptionState&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return RejectedPromise(script_state_, DOMExceptionCode::kNotSupportedError,
                         "tune() is not implemented yet");
}

ScriptPromise VideoEncoder::configure(const VideoEncoderInit* init,
                                      ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* tune_options = init->tuneOptions();
  DCHECK(tune_options);

  if (tune_options->height() == 0) {
    return RejectedPromise(script_state_, DOMExceptionCode::kInvalidStateError,
                           "Invalid height.");
  }

  if (tune_options->width() == 0) {
    return RejectedPromise(script_state_, DOMExceptionCode::kInvalidStateError,
                           "Invalid width.");
  }

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kConfigure;
  request->config = init;
  return EnqueueRequest(request);
}

ScriptPromise VideoEncoder::encode(const VideoFrame* frame,
                                   const VideoEncoderEncodeOptions* opts,
                                   ExceptionState&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!media_encoder_) {
    return RejectedPromise(script_state_, DOMExceptionCode::kInvalidStateError,
                           "Encoder is not configured yet.");
  }
  if (frame->visibleWidth() != uint32_t{frame_size_.width()} ||
      frame->visibleHeight() != uint32_t{frame_size_.height()}) {
    return RejectedPromise(
        script_state_, DOMExceptionCode::kOperationError,
        "Frame size doesn't match initial encoder parameters.");
  }

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kEncode;
  request->frame = frame;
  request->encodeOpts = opts;
  return EnqueueRequest(request);
}

ScriptPromise VideoEncoder::close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!media_encoder_)
    return ScriptPromise::CastUndefined(script_state_);

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kClose;
  return EnqueueRequest(request);
}

ScriptPromise VideoEncoder::flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!media_encoder_) {
    return RejectedPromise(script_state_, DOMExceptionCode::kInvalidStateError,
                           "Encoder is not configured yet.");
  }

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kFlush;
  return EnqueueRequest(request);
}

void VideoEncoder::CallOutputCallback(EncodedVideoChunk* chunk) {
  if (!script_state_->ContextIsValid() || !output_callback_)
    return;
  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk);
}

void VideoEncoder::CallErrorCallback(DOMException* ex) {
  if (!script_state_->ContextIsValid() || !error_callback_)
    return;
  ScriptState::Scope scope(script_state_);
  error_callback_->InvokeAndReportException(nullptr, ex);
}

ScriptPromise VideoEncoder::EnqueueRequest(Request* request) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  request->resolver = resolver;
  requests_.push_back(request);

  if (requests_.size() == 1)
    ProcessRequests();

  return resolver->Promise();
}

void VideoEncoder::ProcessRequests() {
  if (requests_.empty())
    return;

  Request* request = requests_.TakeFirst();
  DCHECK(request);
  DCHECK(request->resolver);
  switch (request->type) {
    case Request::Type::kConfigure:
      ProcessConfigure(request);
      break;
    case Request::Type::kEncode:
      ProcessEncode(request);
      break;
    case Request::Type::kFlush:
      ProcessFlush(request);
      break;
    case Request::Type::kClose:
      ProcessClose(request);
      break;
    default:
      NOTREACHED();
  }
}

void VideoEncoder::ProcessEncode(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(request->type, Request::Type::kEncode);
  DCHECK(media_encoder_);

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    DCHECK(req->resolver);
    if (!self)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (status.is_ok()) {
      req->Resolve();
    } else {
      std::string msg = "Encoding error: " + status.message();
      auto* ex = req->Reject(DOMExceptionCode::kOperationError, msg.c_str());
      self->CallErrorCallback(ex);
    }
    self->ProcessRequests();
  };

  bool keyframe =
      request->encodeOpts->hasKeyFrame() && request->encodeOpts->keyFrame();
  media_encoder_->Encode(request->frame->frame(), keyframe,
                         WTF::Bind(done_callback, WrapWeakPersistent(this),
                                   WrapPersistentIfNeeded(request)));
}

void VideoEncoder::ProcessConfigure(Request* request) {
  DCHECK(request->config);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (media_encoder_) {
    request->Reject(DOMExceptionCode::kOperationError,
                    "Encoder has already been congfigured");
    return;
  }

  auto codec_type = media::StringToVideoCodec(request->config->codec().Utf8());
  if (codec_type == media::kUnknownVideoCodec) {
    request->Reject(DOMExceptionCode::kNotFoundError, "Unknown codec type");
    return;
  }
  media::VideoCodecProfile profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
#if BUILDFLAG(ENABLE_LIBVPX)
  if (codec_type == media::kCodecVP8) {
    media_encoder_ = std::make_unique<media::VpxVideoEncoder>();
    profile = media::VP8PROFILE_ANY;
  } else if (codec_type == media::kCodecVP9) {
    uint8_t level = 0;
    media::VideoColorSpace color_space;
    if (!ParseNewStyleVp9CodecID(request->config->profile().Utf8(), &profile,
                                 &level, &color_space)) {
      request->Reject(DOMExceptionCode::kNotFoundError, "Invalid vp9 profile");
      return;
    }
    media_encoder_ = std::make_unique<media::VpxVideoEncoder>();
  }
#endif  // BUILDFLAG(ENABLE_LIBVPX)

  if (!media_encoder_) {
    request->Reject(DOMExceptionCode::kNotFoundError, "Unsupported codec type");
    return;
  }

  auto* tune_options = request->config->tuneOptions();
  output_callback_ = request->config->output();
  error_callback_ = request->config->error();
  frame_size_ = gfx::Size(tune_options->width(), tune_options->height());

  auto output_cb = WTF::BindRepeating(&VideoEncoder::MediaEncoderOutputCallback,
                                      WrapWeakPersistent(this));

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    DCHECK(req->resolver);
    if (!self)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (status.is_ok()) {
      req->Resolve();
    } else {
      self->media_encoder_.reset();
      self->output_callback_.Clear();
      self->error_callback_.Clear();
      std::string msg = "Encoder initialization error: " + status.message();
      req->Reject(DOMExceptionCode::kOperationError, msg.c_str());
    }
  };

  media::VideoEncoder::Options options;
  options.bitrate = tune_options->bitrate();
  options.height = frame_size_.height();
  options.width = frame_size_.width();
  options.framerate = tune_options->framerate();
  options.threads = 1;
  media_encoder_->Initialize(profile, options, output_cb,
                             WTF::Bind(done_callback, WrapWeakPersistent(this),
                                       WrapPersistent(request)));
}

void VideoEncoder::ProcessClose(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(request->type, Request::Type::kClose);

  media_encoder_.reset();
  output_callback_.Clear();
  error_callback_.Clear();
  request->Resolve();

  while (!requests_.empty()) {
    Request* pending_req = requests_.TakeFirst();
    DCHECK(pending_req);
    DCHECK(pending_req->resolver);
    pending_req->Reject(DOMExceptionCode::kOperationError, "Encoder closed.");
  }
}

void VideoEncoder::ProcessFlush(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(request->type, Request::Type::kFlush);
  DCHECK(media_encoder_);

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    DCHECK(req->resolver);
    if (!self)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (status.is_ok()) {
      req->Resolve();
    } else {
      std::string msg = "Flushing error: " + status.message();
      auto* ex = req->Reject(DOMExceptionCode::kOperationError, msg.c_str());
      self->CallErrorCallback(ex);
    }
    self->ProcessRequests();
  };

  media_encoder_->Flush(WTF::Bind(done_callback, WrapWeakPersistent(this),
                                  WrapPersistentIfNeeded(request)));
}

void VideoEncoder::MediaEncoderOutputCallback(
    media::VideoEncoderOutput output) {
  EncodedVideoMetadata metadata;
  metadata.timestamp = output.timestamp;
  metadata.key_frame = output.key_frame;
  auto deleter = [](void* data, size_t length, void*) {
    delete[] static_cast<uint8_t*>(data);
  };
  ArrayBufferContents data(output.data.release(), output.size, deleter);
  auto* dom_array = MakeGarbageCollected<DOMArrayBuffer>(std::move(data));
  auto* chunk = MakeGarbageCollected<EncodedVideoChunk>(metadata, dom_array);
  CallOutputCallback(chunk);
}

void VideoEncoder::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(output_callback_);
  visitor->Trace(error_callback_);
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
}

void VideoEncoder::Request::Trace(Visitor* visitor) {
  visitor->Trace(config);
  visitor->Trace(frame);
  visitor->Trace(encodeOpts);
  visitor->Trace(resolver);
}

DOMException* VideoEncoder::Request::Reject(DOMExceptionCode code,
                                            const String& message) {
  auto* ex = MakeGarbageCollected<DOMException>(code, message);
  resolver.Release()->Reject(ex);
  return ex;
}
void VideoEncoder::Request::Resolve() {
  resolver.Release()->Resolve();
}

}  // namespace blink
