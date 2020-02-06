// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder_init_parameters.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

// TODO(sandersd): Tune this number.
// Desired number of chunks queued in |writable_sink_|.
constexpr size_t kDesiredInputQueueSize = 4;

// TODO(sandersd): Tune this number.
// Desired number of pending decodes + chunks queued in |readable_source_|.
constexpr size_t kDesiredInternalQueueSize = 4;

std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
    ScriptState* script_state) {
  return nullptr;
}

}  // namespace

class VideoDecoder::WritableSink final : public UnderlyingSinkBase {
 public:
  explicit WritableSink(VideoDecoder* parent) : parent_(parent) {}

  // UnderlyingSinkBase overrides.
  ScriptPromise start(ScriptState*,
                      WritableStreamDefaultController*,
                      ExceptionState& exception_state) override {
    return parent_->Start(exception_state);
  }

  ScriptPromise write(ScriptState*,
                      ScriptValue chunk,
                      WritableStreamDefaultController*,
                      ExceptionState& exception_state) override {
    return parent_->Write(chunk, exception_state);
  }

  ScriptPromise close(ScriptState*, ExceptionState& exception_state) override {
    return parent_->Close(exception_state);
  }

  ScriptPromise abort(ScriptState*,
                      ScriptValue reason,
                      ExceptionState& exception_state) override {
    return parent_->Abort(exception_state);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(parent_);
    UnderlyingSinkBase::Trace(visitor);
  }

 private:
  friend class VideoDecoder;
  Member<VideoDecoder> parent_;
};

class VideoDecoder::ReadableSource final : public UnderlyingSourceBase {
 public:
  ReadableSource(ScriptState* script_state, VideoDecoder* parent)
      : UnderlyingSourceBase(script_state), parent_(parent) {}

  // UnderlyingSourceBase overrides.
  ScriptPromise pull(ScriptState*) override { return parent_->Pull(); }

  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override {
    return parent_->Cancel();
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(parent_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  friend class VideoDecoder;
  Member<VideoDecoder> parent_;
};

// static
VideoDecoder* VideoDecoder::Create(ScriptState* script_state,
                                   ExceptionState& exception_state) {
  return MakeGarbageCollected<VideoDecoder>(script_state, exception_state);
}

VideoDecoder::VideoDecoder(ScriptState* script_state,
                           ExceptionState& exception_state)
    : script_state_(script_state), weak_factory_(this) {
  DVLOG(1) << __func__;
  weak_this_ = weak_factory_.GetWeakPtr();

  WritableSink* writable_sink = MakeGarbageCollected<WritableSink>(this);
  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state, writable_sink, kDesiredInputQueueSize);

  readable_source_ = MakeGarbageCollected<ReadableSource>(script_state_, this);
  readable_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state_, readable_source_, kDesiredInternalQueueSize);
}

VideoDecoder::~VideoDecoder() {
  DVLOG(1) << __func__;
  // TODO(sandersd): Should we reject outstanding promises?
}

ScriptPromise VideoDecoder::CreateWritePromise() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!write_resolver_);

  ScriptPromiseResolver* write_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  write_resolver_ = write_resolver;

  // Note: may release |write_resolver_|.
  MaybeAcceptWrite();

  return write_resolver->Promise();
}

void VideoDecoder::MaybeAcceptWrite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!has_error_);

  if (write_resolver_ && initialized_ &&
      pending_decodes_ < decoder_->GetMaxDecodeRequests() &&
      pending_decodes_ < readable_source_->Controller()->DesiredSize()) {
    write_resolver_.Release()->Resolve();
  }
}

void VideoDecoder::HandleError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  has_error_ = true;

  // TODO(sandersd): Reject other outstanding promises, error the output stream,
  // etc.
}

ReadableStream* VideoDecoder::readable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return readable_;
}

WritableStream* VideoDecoder::writable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return writable_;
}

ScriptPromise VideoDecoder::initialize(const VideoDecoderInitParameters* params,
                                       ExceptionState& exception_state) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (decoder_) {
    // TODO(sandersd): Reinitialization.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Not implemented yet.");
    return ScriptPromise();
  }

  decoder_ = CreateVideoDecoder(script_state_);
  if (!decoder_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "No codec available.");
    return ScriptPromise();
  }

  // VideoDecoder::Initialize() may call OnInitializeDone() reentrantly, in
  // which case |initialize_resolver_| will be nullptr.
  DCHECK(!initialize_resolver_);
  ScriptPromiseResolver* initialize_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  initialize_resolver_ = initialize_resolver;

  // TODO(sandersd): Convert |params| to VideoDecoderConfig.
  // TODO(sandersd): Support |waiting_cb|.
  decoder_->Initialize(
      media::VideoDecoderConfig(
          media::kCodecH264, media::H264PROFILE_BASELINE,
          media::VideoDecoderConfig::AlphaMode::kIsOpaque,
          media::VideoColorSpace::REC709(), media::kNoTransformation,
          gfx::Size(320, 180), gfx::Rect(0, 0, 320, 180), gfx::Size(320, 180),
          media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted),
      false, nullptr, WTF::Bind(&VideoDecoder::OnInitializeDone, weak_this_),
      WTF::BindRepeating(&VideoDecoder::OnOutput, weak_this_),
      base::RepeatingCallback<void(media::WaitingReason)>());

  return initialize_resolver->Promise();
}

void VideoDecoder::OnInitializeDone(bool success) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    initialize_resolver_.Release()->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, "Initialization failed."));
    HandleError();
    return;
  }

  initialized_ = true;
  initialize_resolver_.Release()->Resolve();
  MaybeAcceptWrite();
}

void VideoDecoder::OnDecodeDone(media::DecodeStatus status) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != media::DecodeStatus::OK) {
    // TODO(sandersd): Handle ABORTED during Reset.
    HandleError();
    return;
  }

  pending_decodes_--;
  MaybeAcceptWrite();
}

void VideoDecoder::OnOutput(scoped_refptr<media::VideoFrame> frame) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  readable_source_->Controller()->Enqueue(ScriptValue::From(
      script_state_, MakeGarbageCollected<VideoFrame>(frame)));
}

ScriptPromise VideoDecoder::Start(ExceptionState&) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateWritePromise();
}

ScriptPromise VideoDecoder::Write(ScriptValue chunk,
                                  ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Convert |chunk| to EncodedVideoChunk.
  EncodedVideoChunk* encoded_video_chunk =
      V8EncodedVideoChunk::ToImplWithTypeCheck(script_state_->GetIsolate(),
                                               chunk.V8Value());
  if (!encoded_video_chunk) {
    // TODO(sandersd): Set |has_error| and reject promises.
    exception_state.ThrowTypeError("Chunk is not an EncodedVideoChunk.");
    return ScriptPromise();
  }

  // Convert |encoded_video_chunk| to DecoderBuffer.
  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      media::DecoderBuffer::CopyFrom(
          static_cast<uint8_t*>(encoded_video_chunk->data()->Data()),
          encoded_video_chunk->data()->ByteLengthAsSizeT());
  decoder_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(encoded_video_chunk->timestamp()));
  // TODO(sandersd): Should a null duration be converted to kNoTimestamp?
  bool is_null = false;
  decoder_buffer->set_duration(base::TimeDelta::FromMicroseconds(
      encoded_video_chunk->duration(&is_null)));
  decoder_buffer->set_is_key_frame(encoded_video_chunk->type() == "key");

  // TODO(sandersd): Add reentrancy checker; OnDecodeDone() could disturb
  // |pending_decodes_|.
  pending_decodes_++;
  decoder_->Decode(std::move(decoder_buffer),
                   WTF::Bind(&VideoDecoder::OnDecodeDone, weak_this_));
  return CreateWritePromise();
}

ScriptPromise VideoDecoder::Close(ExceptionState& exception_state) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(sandersd): Flush.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented yet.");
  return ScriptPromise();
}

ScriptPromise VideoDecoder::Abort(ExceptionState& exception_state) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(sandersd): Reset.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented yet.");
  return ScriptPromise();
}

ScriptPromise VideoDecoder::Pull() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MaybeAcceptWrite();
  return ScriptPromise::CastUndefined(script_state_);
}

ScriptPromise VideoDecoder::Cancel() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(sandersd): Close or abort the source.
  return ScriptPromise::RejectWithDOMException(
      script_state_,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                         "Not implemented yet."));
}

void VideoDecoder::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(readable_source_);
  visitor->Trace(readable_);
  visitor->Trace(writable_);
  visitor->Trace(initialize_resolver_);
  visitor->Trace(write_resolver_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
