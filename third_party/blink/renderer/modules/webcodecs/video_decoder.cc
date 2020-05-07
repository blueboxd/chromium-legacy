// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/media_buildflags.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_config.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder_init.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

media::VideoDecoderConfig ToVideoDecoderConfig(
    const EncodedVideoConfig* config) {
  std::vector<uint8_t> extra_data;
  if (config->hasDescription()) {
    DOMArrayBuffer* buffer;
    if (config->description().IsArrayBuffer()) {
      buffer = config->description().GetAsArrayBuffer();
    } else {
      // TODO(sandersd): Can IsNull() be true?
      DCHECK(config->description().IsArrayBufferView());
      buffer = config->description().GetAsArrayBufferView()->buffer();
    }
    // TODO(sandersd): Is it possible to not have Data()?
    uint8_t* start = static_cast<uint8_t*>(buffer->Data());
    size_t size = buffer->ByteLengthAsSizeT();
    extra_data.assign(start, start + size);
  }

  // TODO(sandersd): Parse |codec| to produce a VideoCodecProfile.
  media::VideoCodec codec = media::kCodecH264;
  media::VideoCodecProfile profile = media::H264PROFILE_BASELINE;
  // TODO(sandersd): Either remove sizes from VideoDecoderConfig (replace with
  // sample aspect) or parse the AvcC here to get the actual size.
  gfx::Size size = gfx::Size(1280, 720);
  return media::VideoDecoderConfig(
      codec, profile, media::VideoDecoderConfig::AlphaMode::kIsOpaque,
      media::VideoColorSpace::REC709(), media::kNoTransformation, size,
      gfx::Rect(gfx::Point(), size), size, extra_data,
      media::EncryptionScheme::kUnencrypted);
}

std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
    media::MediaLog* media_log) {
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  return std::make_unique<media::FFmpegVideoDecoder>(media_log);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
}

}  // namespace

// static
VideoDecoder* VideoDecoder::Create(ScriptState* script_state,
                                   const VideoDecoderInit* init,
                                   ExceptionState& exception_state) {
  return MakeGarbageCollected<VideoDecoder>(script_state, init,
                                            exception_state);
}

VideoDecoder::VideoDecoder(ScriptState* script_state,
                           const VideoDecoderInit* init,
                           ExceptionState& exception_state)
    : script_state_(script_state) {
  DVLOG(1) << __func__;
  // TODO(sandersd): Is it an error to not provide all callbacks?
  output_cb_ = init->output();
  error_cb_ = init->error();
}

VideoDecoder::~VideoDecoder() {
  DVLOG(1) << __func__;
}

int32_t VideoDecoder::decodeQueueSize() {
  return requested_decodes_;
}

int32_t VideoDecoder::decodeProcessingCount() {
  return pending_decodes_.size();
}

ScriptPromise VideoDecoder::configure(const EncodedVideoConfig* config,
                                      ExceptionState&) {
  DVLOG(1) << __func__;
  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kConfigure;
  request->config = config;
  return EnqueueRequest(request);
}

ScriptPromise VideoDecoder::decode(const EncodedVideoChunk* chunk,
                                   ExceptionState&) {
  DVLOG(3) << __func__;
  requested_decodes_++;
  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kDecode;
  request->chunk = chunk;
  return EnqueueRequest(request);
}

ScriptPromise VideoDecoder::flush(ExceptionState&) {
  DVLOG(3) << __func__;
  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kFlush;
  return EnqueueRequest(request);
}

ScriptPromise VideoDecoder::reset(ExceptionState&) {
  DVLOG(3) << __func__;
  requested_resets_++;
  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kReset;
  return EnqueueRequest(request);
}

ScriptPromise VideoDecoder::EnqueueRequest(Request* request) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  request->resolver = resolver;
  requests_.push_back(request);
  ProcessRequests();
  return resolver->Promise();
}

void VideoDecoder::ProcessRequests() {
  DVLOG(3) << __func__;
  // TODO(sandersd): Re-entrancy checker.
  while (!pending_request_ && !requests_.IsEmpty()) {
    Request* request = requests_.front();
    switch (request->type) {
      case Request::Type::kConfigure:
        if (!ProcessConfigureRequest(request))
          return;
        break;
      case Request::Type::kDecode:
        if (!ProcessDecodeRequest(request))
          return;
        break;
      case Request::Type::kFlush:
        if (!ProcessFlushRequest(request))
          return;
        break;
      case Request::Type::kReset:
        if (!ProcessResetRequest(request))
          return;
        break;
    }
    requests_.pop_front();
  }
}

bool VideoDecoder::ProcessConfigureRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK(request->config);

  // TODO(sandersd): If we require configure() after reset() and there is a
  // pending reset, then we could drop this request.
  // TODO(sandersd): If the next request is also a configure(), they can be
  // merged. It's not trivial to detect that situation.
  // TODO(sandersd): Elide this request if the configuration is unchanged.

  if (!decoder_) {
    media_log_ = std::make_unique<media::NullMediaLog>();
    decoder_ = CreateVideoDecoder(media_log_.get());
    if (!decoder_) {
      request->resolver.Release()->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Codec initialization failed."));
      // TODO(sandersd): This is a bit awkward because |request| is still in the
      // queue.
      HandleError();
      return false;
    }

    // Processing continues in OnInitializeDone().
    // TODO(sandersd): OnInitializeDone() may be called reentrantly, in which
    // case it must not call ProcessRequests().
    pending_request_ = request;
    decoder_->Initialize(
        ToVideoDecoderConfig(pending_request_->config), false, nullptr,
        WTF::Bind(&VideoDecoder::OnInitializeDone, WrapWeakPersistent(this)),
        WTF::BindRepeating(&VideoDecoder::OnOutput, WrapWeakPersistent(this)),
        media::WaitingCB());
    return true;
  }

  // Note: This flush must not be elided when there is a pending reset. An
  // alternative would be to process Reset() requests immediately, then process
  // already queued requests in a special mode. It seems easier to drop all of
  // this and require configure() after reset() instead.
  if (pending_decodes_.size() + 1 > size_t{decoder_->GetMaxDecodeRequests()}) {
    // Try again after OnDecodeDone().
    return false;
  }

  // Processing continues in OnConfigureFlushDone().
  pending_request_ = request;
  decoder_->Decode(
      media::DecoderBuffer::CreateEOSBuffer(),
      WTF::Bind(&VideoDecoder::OnConfigureFlushDone, WrapWeakPersistent(this)));
  return true;
}

bool VideoDecoder::ProcessDecodeRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kDecode);
  DCHECK(request->chunk);
  DCHECK_GT(requested_decodes_, 0);

  // TODO(sandersd): If a reset has been requested, resolve immediately.

  if (!decoder_) {
    // TODO(sandersd): Add explanation (no valid configuration).
    request->resolver.Release()->Reject();
    return true;
  }

  if (pending_decodes_.size() + 1 > size_t{decoder_->GetMaxDecodeRequests()}) {
    // Try again after OnDecodeDone().
    return false;
  }

  // Convert |chunk| to a DecoderBuffer.
  auto decoder_buffer = media::DecoderBuffer::CopyFrom(
      static_cast<uint8_t*>(request->chunk->data()->Data()),
      request->chunk->data()->ByteLengthAsSizeT());
  decoder_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(request->chunk->timestamp()));
  // TODO(sandersd): Use kUnknownTimestamp instead of 0?
  decoder_buffer->set_duration(base::TimeDelta::FromMicroseconds(
      request->chunk->duration().value_or(0)));
  decoder_buffer->set_is_key_frame(request->chunk->type() == "key");

  // Submit for decoding.
  // |pending_decode_id_| must not be zero because it is used as a key in a
  // HeapHashMap (pending_decodes_).
  while (++pending_decode_id_ == 0 ||
         pending_decodes_.Contains(pending_decode_id_))
    ;
  pending_decodes_.Set(pending_decode_id_, request);
  --requested_decodes_;
  decoder_->Decode(std::move(decoder_buffer),
                   WTF::Bind(&VideoDecoder::OnDecodeDone,
                             WrapWeakPersistent(this), pending_decode_id_));
  return true;
}

bool VideoDecoder::ProcessFlushRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kFlush);

  // TODO(sandersd): If a reset has been requested, resolve immediately.

  if (!decoder_) {
    // TODO(sandersd): Maybe it is valid to flush no decoder? If not, it may be
    // necessary to enter a full error state here.
    request->resolver.Release()->Reject();
    return true;
  }

  if (pending_decodes_.size() + 1 > size_t{decoder_->GetMaxDecodeRequests()}) {
    // Try again after OnDecodeDone().
    return false;
  }

  // Processing continues in OnFlushDone().
  pending_request_ = request;
  decoder_->Decode(
      media::DecoderBuffer::CreateEOSBuffer(),
      WTF::Bind(&VideoDecoder::OnFlushDone, WrapWeakPersistent(this)));
  return true;
}

bool VideoDecoder::ProcessResetRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kReset);
  DCHECK_GT(requested_resets_, 0);

  // Processing continues in OnResetDone().
  pending_request_ = request;
  --requested_resets_;
  decoder_->Reset(
      WTF::Bind(&VideoDecoder::OnResetDone, WrapWeakPersistent(this)));
  return true;
}

void VideoDecoder::HandleError() {
  // TODO(sandersd): Reject outstanding requests. We can stop rejeting at a
  // decode(keyframe), reset(), or configure(), but maybe we should reject
  // everything already queued (an implicit reset).
  NOTIMPLEMENTED();
}

void VideoDecoder::OnConfigureFlushDone(media::DecodeStatus status) {
  DVLOG(3) << __func__;
  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kConfigure);

  if (status != media::DecodeStatus::OK) {
    HandleError();
    return;
  }

  // Processing continues in OnInitializeDone().
  decoder_->Initialize(
      ToVideoDecoderConfig(pending_request_->config), false, nullptr,
      WTF::Bind(&VideoDecoder::OnInitializeDone, WrapWeakPersistent(this)),
      WTF::BindRepeating(&VideoDecoder::OnOutput, WrapWeakPersistent(this)),
      media::WaitingCB());
}

void VideoDecoder::OnInitializeDone(media::Status status) {
  DVLOG(3) << __func__;
  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kConfigure);

  if (!status.is_ok()) {
    // TODO(tmathmeyer) this drops the media error - should we consider logging
    // it or converting it to the DOMException type somehow?
    pending_request_.Release()->resolver.Release()->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                           "Codec initialization failed."));
    HandleError();
    return;
  }

  pending_request_.Release()->resolver.Release()->Resolve();
  ProcessRequests();
}

void VideoDecoder::OnDecodeDone(uint32_t id, media::DecodeStatus status) {
  DVLOG(3) << __func__;
  DCHECK(pending_decodes_.Contains(id));

  if (status != media::DecodeStatus::OK) {
    // TODO(sandersd): Handle ABORTED.
    HandleError();
    return;
  }

  auto it = pending_decodes_.find(id);
  it->value->resolver.Release()->Resolve();
  pending_decodes_.erase(it);
  ProcessRequests();
}

void VideoDecoder::OnFlushDone(media::DecodeStatus status) {
  DVLOG(3) << __func__;
  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kFlush);

  if (status != media::DecodeStatus::OK) {
    HandleError();
    return;
  }

  pending_request_.Release()->resolver.Release()->Resolve();
  ProcessRequests();
}

void VideoDecoder::OnResetDone() {
  DVLOG(3) << __func__;
  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kReset);

  pending_request_.Release()->resolver.Release()->Resolve();
  ProcessRequests();
}

void VideoDecoder::OnOutput(scoped_refptr<media::VideoFrame> frame) {
  DVLOG(3) << __func__;
  output_cb_->InvokeAndReportException(nullptr,
                                       MakeGarbageCollected<VideoFrame>(frame));
}

void VideoDecoder::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(output_cb_);
  visitor->Trace(error_cb_);
  visitor->Trace(requests_);
  visitor->Trace(pending_request_);
  visitor->Trace(pending_decodes_);
  ScriptWrappable::Trace(visitor);
}

void VideoDecoder::Request::Trace(Visitor* visitor) {
  visitor->Trace(config);
  visitor->Trace(chunk);
  visitor->Trace(resolver);
}

}  // namespace blink
