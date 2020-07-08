// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/decoder_template.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/media_util.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

template <typename Traits>
DecoderTemplate<Traits>::DecoderTemplate(ScriptState* script_state,
                                         const InitType* init,
                                         ExceptionState& exception_state)
    : script_state_(script_state) {
  DVLOG(1) << __func__;
  // TODO(sandersd): Is it an error to not provide all callbacks?
  output_cb_ = init->output();
  error_cb_ = init->error();
}

template <typename Traits>
DecoderTemplate<Traits>::~DecoderTemplate() {
  DVLOG(1) << __func__;
}

template <typename Traits>
int32_t DecoderTemplate<Traits>::decodeQueueSize() {
  return requested_decodes_;
}

template <typename Traits>
void DecoderTemplate<Traits>::configure(const ConfigType* config,
                                        ExceptionState&) {
  DVLOG(1) << __func__;
  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kConfigure;
  request->config = config;
  requests_.push_back(request);
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::decode(const InputType* chunk, ExceptionState&) {
  DVLOG(3) << __func__;
  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kDecode;
  request->chunk = chunk;
  requests_.push_back(request);
  ++requested_decodes_;
  ProcessRequests();
}

template <typename Traits>
ScriptPromise DecoderTemplate<Traits>::flush(ExceptionState&) {
  DVLOG(3) << __func__;
  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kFlush;
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  request->resolver = resolver;
  requests_.push_back(request);
  ProcessRequests();
  return resolver->Promise();
}

template <typename Traits>
void DecoderTemplate<Traits>::reset(ExceptionState&) {
  DVLOG(3) << __func__;
  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kReset;
  requests_.push_back(request);
  ++requested_resets_;
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::close() {
  // TODO(chcunningham): Implement.
}

template <typename Traits>
void DecoderTemplate<Traits>::ProcessRequests() {
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

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessConfigureRequest(Request* request) {
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
    decoder_ = Traits::CreateDecoder(*ExecutionContext::From(script_state_),
                                     media_log_.get());
    if (!decoder_) {
      // TODO(sandersd): This is a bit awkward because |request| is still in the
      // queue.
      HandleError();
      return false;
    }

    // Processing continues in OnInitializeDone().
    // TODO(sandersd): OnInitializeDone() may be called reentrantly, in which
    // case it must not call ProcessRequests().
    pending_request_ = request;
    Traits::InitializeDecoder(
        *decoder_, *pending_request_->config,
        WTF::Bind(&DecoderTemplate::OnInitializeDone, WrapWeakPersistent(this)),
        WTF::BindRepeating(&DecoderTemplate::OnOutput,
                           WrapWeakPersistent(this)));
    return true;
  }

  // Note: This flush must not be elided when there is a pending reset. An
  // alternative would be to process Reset() requests immediately, then process
  // already queued requests in a special mode. It seems easier to drop all of
  // this and require configure() after reset() instead.
  if (pending_decodes_.size() + 1 >
      size_t{Traits::GetMaxDecodeRequests(*decoder_)}) {
    // Try again after OnDecodeDone().
    return false;
  }

  // Processing continues in OnConfigureFlushDone().
  pending_request_ = request;
  decoder_->Decode(media::DecoderBuffer::CreateEOSBuffer(),
                   WTF::Bind(&DecoderTemplate::OnConfigureFlushDone,
                             WrapWeakPersistent(this)));
  return true;
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessDecodeRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kDecode);
  DCHECK(request->chunk);
  DCHECK_GT(requested_decodes_, 0);

  // TODO(sandersd): If a reset has been requested, complete immediately.

  if (!decoder_) {
    // TODO(sandersd): Emit an error?
    return true;
  }

  if (pending_decodes_.size() + 1 >
      size_t{Traits::GetMaxDecodeRequests(*decoder_)}) {
    // Try again after OnDecodeDone().
    return false;
  }

  // Submit for decoding.
  // |pending_decode_id_| must not be zero because it is used as a key in a
  // HeapHashMap (pending_decodes_).
  while (++pending_decode_id_ == 0 ||
         pending_decodes_.Contains(pending_decode_id_))
    ;
  pending_decodes_.Set(pending_decode_id_, request);
  --requested_decodes_;
  decoder_->Decode(std::move(Traits::MakeDecoderBuffer(*request->chunk)),
                   WTF::Bind(&DecoderTemplate::OnDecodeDone,
                             WrapWeakPersistent(this), pending_decode_id_));
  return true;
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessFlushRequest(Request* request) {
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

  if (pending_decodes_.size() + 1 >
      size_t{Traits::GetMaxDecodeRequests(*decoder_)}) {
    // Try again after OnDecodeDone().
    return false;
  }

  // Processing continues in OnFlushDone().
  pending_request_ = request;
  decoder_->Decode(
      media::DecoderBuffer::CreateEOSBuffer(),
      WTF::Bind(&DecoderTemplate::OnFlushDone, WrapWeakPersistent(this)));
  return true;
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessResetRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kReset);
  DCHECK_GT(requested_resets_, 0);

  // Processing continues in OnResetDone().
  pending_request_ = request;
  --requested_resets_;
  decoder_->Reset(
      WTF::Bind(&DecoderTemplate::OnResetDone, WrapWeakPersistent(this)));
  return true;
}

template <typename Traits>
void DecoderTemplate<Traits>::HandleError() {
  // TODO(sandersd): Reject outstanding requests. We can stop rejeting at a
  // decode(keyframe), reset(), or configure(), but maybe we should reject
  // everything already queued (an implicit reset).
  NOTIMPLEMENTED();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnConfigureFlushDone(media::DecodeStatus status) {
  DVLOG(3) << __func__;
  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kConfigure);

  if (status != media::DecodeStatus::OK) {
    HandleError();
    return;
  }

  // Processing continues in OnInitializeDone().
  Traits::InitializeDecoder(
      *decoder_, *pending_request_->config,
      WTF::Bind(&DecoderTemplate::OnInitializeDone, WrapWeakPersistent(this)),
      WTF::BindRepeating(&DecoderTemplate::OnOutput, WrapWeakPersistent(this)));
}

template <typename Traits>
void DecoderTemplate<Traits>::OnInitializeDone(media::Status status) {
  DVLOG(3) << __func__;
  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kConfigure);

  if (!status.is_ok()) {
    // TODO(tmathmeyer): this drops the media error - should we consider logging
    // it or converting it to the DOMException type somehow?
    HandleError();
    return;
  }

  pending_request_.Release();
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnDecodeDone(uint32_t id,
                                           media::DecodeStatus status) {
  DVLOG(3) << __func__;
  DCHECK(pending_decodes_.Contains(id));

  if (status != media::DecodeStatus::OK) {
    // TODO(sandersd): Handle ABORTED.
    HandleError();
    return;
  }

  auto it = pending_decodes_.find(id);
  pending_decodes_.erase(it);
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnFlushDone(media::DecodeStatus status) {
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

template <typename Traits>
void DecoderTemplate<Traits>::OnResetDone() {
  DVLOG(3) << __func__;
  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kReset);

  pending_request_.Release();
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnOutput(scoped_refptr<MediaOutputType> output) {
  DVLOG(3) << __func__;
  output_cb_->InvokeAndReportException(
      nullptr, MakeGarbageCollected<OutputType>(output));
}

template <typename Traits>
void DecoderTemplate<Traits>::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(output_cb_);
  visitor->Trace(error_cb_);
  visitor->Trace(requests_);
  visitor->Trace(pending_request_);
  visitor->Trace(pending_decodes_);
  ScriptWrappable::Trace(visitor);
}

template <typename Traits>
void DecoderTemplate<Traits>::Request::Trace(Visitor* visitor) const {
  visitor->Trace(config);
  visitor->Trace(chunk);
  visitor->Trace(resolver);
}

template class DecoderTemplate<AudioDecoderTraits>;
template class DecoderTemplate<VideoDecoderTraits>;

}  // namespace blink
