// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_config.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

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
  // TODO(sandersd): Extract callbacks from |init|.
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

  // If there were no requests before, trigger request processing.
  if (requests_.size() == 1)
    ProcessRequests();

  return resolver->Promise();
}

void VideoDecoder::ProcessRequests() {}

void VideoDecoder::HandleError() {}

void VideoDecoder::OnInitializeDone(media::Status status) {
  DVLOG(3) << __func__;

  Request* request = requests_.TakeFirst();
  DCHECK_EQ(request->type, Request::Type::kConfigure);

  if (!status.is_ok()) {
    // TODO(tmathmeyer) this drops the media error - should we consider logging
    // it or converting it to the DOMException type somehow?
    request->resolver.Release()->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, "Codec initialization failed."));
    HandleError();
    return;
  }

  request->resolver.Release()->Resolve();
  ProcessRequests();
}

void VideoDecoder::OnDecodeDone(uint32_t id, media::DecodeStatus status) {
  DVLOG(3) << __func__;
  DCHECK(pending_decodes_.Contains(id));

  if (status != media::DecodeStatus::OK) {
    // TODO(sandersd): Handle ABORTED during Reset.
    HandleError();
    return;
  }

  auto it = pending_decodes_.find(id);
  it->value->resolver.Release()->Resolve();
  pending_decodes_.erase(it);
  ProcessRequests();
}

void VideoDecoder::OnOutput(scoped_refptr<media::VideoFrame> frame) {
  DVLOG(3) << __func__;
  // TODO(sandersd): Call output callback.
  // MakeGarbageCollected<VideoFrame>(frame)
}

void VideoDecoder::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(requests_);
  visitor->Trace(pending_decodes_);
  ScriptWrappable::Trace(visitor);
}

void VideoDecoder::Request::Trace(Visitor* visitor) {
  visitor->Trace(config);
  visitor->Trace(chunk);
  visitor->Trace(resolver);
}

}  // namespace blink
