// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_encoder.h"
#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/video/vpx_video_encoder.h"
#endif
#include "media/base/async_destroy_video_encoder.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator_adapter.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_metadata.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

namespace {
std::unique_ptr<media::VideoEncoder> CreateAcceleratedVideoEncoder() {
  auto* gpu_factories = Platform::Current()->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoAcceleratorEnabled())
    return nullptr;

  auto task_runner = Thread::MainThread()->GetTaskRunner();
  return std::make_unique<
      media::AsyncDestroyVideoEncoder<media::VideoEncodeAcceleratorAdapter>>(
      std::make_unique<media::VideoEncodeAcceleratorAdapter>(
          gpu_factories, std::move(task_runner)));
}

std::unique_ptr<media::VideoEncoder> CreateVpxVideoEncoder() {
#if BUILDFLAG(ENABLE_LIBVPX)
  return std::make_unique<media::VpxVideoEncoder>();
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_LIBVPX)
}

}  // namespace

// static
VideoEncoder* VideoEncoder::Create(ScriptState* script_state,
                                   const VideoEncoderInit* init,
                                   ExceptionState& exception_state) {
  return MakeGarbageCollected<VideoEncoder>(script_state, init,
                                            exception_state);
}

VideoEncoder::VideoEncoder(ScriptState* script_state,
                           const VideoEncoderInit* init,
                           ExceptionState& exception_state)
    : script_state_(script_state) {
  output_callback_ = init->output();
  if (init->hasError())
    error_callback_ = init->error();
}

VideoEncoder::~VideoEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoEncoder::configure(const VideoEncoderConfig* config,
                             ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (config->height() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid height.");
    return;
  }

  if (config->width() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid width.");
    return;
  }

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kConfigure;
  request->config = config;
  EnqueueRequest(request);
}

void VideoEncoder::encode(VideoFrame* frame,
                          const VideoEncoderEncodeOptions* opts,
                          ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!media_encoder_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Encoder is not configured yet.");
    return;
  }
  if (frame->cropWidth() != uint32_t{frame_size_.width()} ||
      frame->cropHeight() != uint32_t{frame_size_.height()}) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Frame size doesn't match initial encoder parameters.");
    return;
  }

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kEncode;
  request->frame = frame;
  request->encodeOpts = opts;
  return EnqueueRequest(request);
}

void VideoEncoder::close(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!media_encoder_)
    return;

  reset(exception_state);
  media_encoder_.reset();
  output_callback_.Clear();
  error_callback_.Clear();
}

ScriptPromise VideoEncoder::flush(ExceptionState&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!media_encoder_) {
    auto* ex = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Encoder is not configured yet.");
    return ScriptPromise::RejectWithDOMException(script_state_, ex);
  }

  Request* request = MakeGarbageCollected<Request>();
  request->resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  request->type = Request::Type::kFlush;
  EnqueueRequest(request);
  return request->resolver->Promise();
}

void VideoEncoder::reset(ExceptionState&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO: Not fully implemented yet

  while (!requests_.empty()) {
    Request* pending_req = requests_.TakeFirst();
    DCHECK(pending_req);
    if (pending_req->resolver) {
      auto* ex = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "reset() was called.");
      pending_req->resolver.Release()->Reject(ex);
    }
  }
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

void VideoEncoder::CallErrorCallback(DOMExceptionCode code,
                                     const String& message) {
  auto* ex = MakeGarbageCollected<DOMException>(code, message);
  CallErrorCallback(ex);
}

void VideoEncoder::EnqueueRequest(Request* request) {
  requests_.push_back(request);
  ProcessRequests();
}

void VideoEncoder::ProcessRequests() {
  while (!requests_.empty() && !stall_request_processing_) {
    Request* request = requests_.TakeFirst();
    DCHECK(request);
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
      default:
        NOTREACHED();
    }
  }
}

void VideoEncoder::ProcessEncode(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(request->type, Request::Type::kEncode);

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    if (!self)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      std::string msg = "Encoding error: " + status.message();
      self->CallErrorCallback(DOMExceptionCode::kOperationError, msg.c_str());
    }
    self->ProcessRequests();
  };

  if (!media_encoder_) {
    CallErrorCallback(DOMExceptionCode::kOperationError,
                      "Encoder is not configured");
    return;
  }

  bool keyframe = request->encodeOpts->hasKeyFrameNonNull() &&
                  request->encodeOpts->keyFrameNonNull();
  media_encoder_->Encode(request->frame->frame(), keyframe,
                         WTF::Bind(done_callback, WrapWeakPersistent(this),
                                   WrapPersistentIfNeeded(request)));
}

void VideoEncoder::ProcessConfigure(Request* request) {
  DCHECK(request->config);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* config = request->config.Get();
  AccelerationPreference acc_pref = AccelerationPreference::kAllow;

  if (media_encoder_) {
    CallErrorCallback(DOMExceptionCode::kOperationError,
                      "Encoder has already been congfigured");
    return;
  }

  if (config->hasAcceleration()) {
    std::string preference = IDLEnumAsString(config->acceleration()).Utf8();
    if (preference == "deny") {
      acc_pref = AccelerationPreference::kDeny;
    } else if (preference == "require") {
      acc_pref = AccelerationPreference::kRequire;
    } else if (preference == "allow") {
      acc_pref = AccelerationPreference::kAllow;
    } else {
      CallErrorCallback(DOMExceptionCode::kNotFoundError,
                        "Unknown acceleration type");
      return;
    }
  }

  std::string codec_str = config->codec().Utf8();
  std::string profile_str = config->profile().Utf8();
  auto codec_type = media::StringToVideoCodec(codec_str);
  if (codec_type == media::kUnknownVideoCodec) {
    CallErrorCallback(DOMExceptionCode::kNotFoundError, "Unknown codec type");
    return;
  }

  // TODO(ezemtsov): Put the encoder creation logic below in a separate class or
  // method.
  media::VideoCodecProfile profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
  if (codec_type == media::kCodecVP8) {
    if (acc_pref == AccelerationPreference::kRequire) {
      CallErrorCallback(DOMExceptionCode::kNotFoundError,
                        "Accelerated vp8 is not supported");
      return;
    }
    media_encoder_ = CreateVpxVideoEncoder();
    profile = media::VP8PROFILE_ANY;
  } else if (codec_type == media::kCodecVP9) {
    uint8_t level = 0;
    media::VideoColorSpace color_space;
    if (!ParseNewStyleVp9CodecID(profile_str, &profile, &level, &color_space)) {
      CallErrorCallback(DOMExceptionCode::kNotFoundError,
                        "Invalid vp9 profile");
      return;
    }
    if (acc_pref == AccelerationPreference::kRequire) {
      CallErrorCallback(DOMExceptionCode::kNotFoundError,
                        "Accelerated vp9 is not supported");
      return;
    }
    media_encoder_ = CreateVpxVideoEncoder();
  } else if (codec_type == media::kCodecH264) {
    codec_type = media::kCodecH264;
    uint8_t level = 0;
    if (!ParseAVCCodecId(profile_str, &profile, &level)) {
      CallErrorCallback(DOMExceptionCode::kNotFoundError,
                        "Invalid AVC profile");
      return;
    }
    if (acc_pref == AccelerationPreference::kDeny) {
      CallErrorCallback(DOMExceptionCode::kNotFoundError,
                        "Software h264 is not supported yet");
      return;
    }
    media_encoder_ = CreateAcceleratedVideoEncoder();
  }

  if (!media_encoder_) {
    CallErrorCallback(DOMExceptionCode::kNotFoundError,
                      "Unsupported codec type");
    return;
  }

  frame_size_ = gfx::Size(config->width(), config->height());

  auto output_cb = WTF::BindRepeating(&VideoEncoder::MediaEncoderOutputCallback,
                                      WrapWeakPersistent(this));

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    if (!self)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->media_encoder_.reset();
      self->output_callback_.Clear();
      self->error_callback_.Clear();
      std::string msg = "Encoder initialization error: " + status.message();
      self->CallErrorCallback(DOMExceptionCode::kOperationError, msg.c_str());
    }
    self->stall_request_processing_ = false;
    self->ProcessRequests();
  };

  media::VideoEncoder::Options options;
  options.bitrate = config->bitrate();
  options.height = frame_size_.height();
  options.width = frame_size_.width();
  options.framerate = config->framerate();
  options.threads = 1;
  stall_request_processing_ = true;
  media_encoder_->Initialize(profile, options, output_cb,
                             WTF::Bind(done_callback, WrapWeakPersistent(this),
                                       WrapPersistent(request)));
}  // namespace blink

void VideoEncoder::ProcessFlush(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(request->type, Request::Type::kFlush);

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    DCHECK(req->resolver);
    if (!self)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (status.is_ok()) {
      req->resolver.Release()->Resolve();
    } else {
      std::string msg = "Flushing error: " + status.message();
      auto* ex = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, msg.c_str());
      self->CallErrorCallback(ex);
      req->resolver.Release()->Reject(ex);
    }
    self->stall_request_processing_ = false;
    self->ProcessRequests();
  };

  if (!media_encoder_) {
    auto* ex = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, "Encoder is not configured");
    CallErrorCallback(ex);
    request->resolver.Release()->Reject(ex);
    return;
  }

  stall_request_processing_ = true;
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

void VideoEncoder::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(output_callback_);
  visitor->Trace(error_callback_);
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
}

void VideoEncoder::Request::Trace(Visitor* visitor) const {
  visitor->Trace(config);
  visitor->Trace(frame);
  visitor->Trace(encodeOpts);
  visitor->Trace(resolver);
}

}  // namespace blink
