// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_

#include <stdint.h>
#include <memory>

#include "media/base/media_log.h"
#include "media/base/status.h"
#include "media/base/video_decoder.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_codecs_error_callback.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class EncodedVideoChunk;
class EncodedVideoConfig;
class ExceptionState;
class ScriptState;
class VideoDecoderInit;

class MODULES_EXPORT VideoDecoder final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoDecoder* Create(ScriptState*,
                              const VideoDecoderInit*,
                              ExceptionState&);

  VideoDecoder(ScriptState*, const VideoDecoderInit*, ExceptionState&);
  ~VideoDecoder() override;

  // video_decoder.idl implementation.
  int32_t decodeQueueSize();
  int32_t decodeProcessingCount();
  ScriptPromise configure(const EncodedVideoConfig*, ExceptionState&);
  ScriptPromise decode(const EncodedVideoChunk*, ExceptionState&);
  ScriptPromise flush(ExceptionState&);
  ScriptPromise reset(ExceptionState&);

  // GarbageCollected override.
  void Trace(Visitor*) override;

 private:
  struct Request : public GarbageCollected<Request> {
    enum class Type {
      kConfigure,
      kDecode,
      kFlush,
      kReset,
    };

    void Trace(Visitor*);

    Type type;
    Member<const EncodedVideoConfig> config;
    Member<const EncodedVideoChunk> chunk;
    Member<ScriptPromiseResolver> resolver;
  };

  ScriptPromise EnqueueRequest(Request* request);
  void ProcessRequests();
  bool ProcessConfigureRequest(Request* request);
  bool ProcessDecodeRequest(Request* request);
  bool ProcessFlushRequest(Request* request);
  bool ProcessResetRequest(Request* request);
  void HandleError();

  // Called by |decoder_|.
  void OnInitializeDone(media::Status status);
  void OnDecodeDone(uint32_t id, media::DecodeStatus);
  void OnFlushDone(media::DecodeStatus);
  void OnConfigureFlushDone(media::DecodeStatus);
  void OnResetDone();
  void OnOutput(scoped_refptr<media::VideoFrame>);

  Member<ScriptState> script_state_;
  Member<V8VideoDecoderOutputCallback> output_cb_;
  Member<V8WebCodecsErrorCallback> error_cb_;

  HeapDeque<Member<Request>> requests_;
  int32_t requested_decodes_ = 0;
  int32_t requested_resets_ = 0;

  // An in-flight, mutually-exclusive request.
  // Could be a configure, flush, or reset. Decodes go in |pending_decodes_|.
  Member<Request> pending_request_;

  std::unique_ptr<media::MediaLog> media_log_;
  // TODO(sandersd): Store the last config, flush, and reset so that duplicates
  // can be elided.
  std::unique_ptr<media::VideoDecoder> decoder_;
  // TODO(sandersd): Can this just be a HashSet by ptr comparison?
  uint32_t pending_decode_id_ = 0;
  HeapHashMap<uint32_t, Member<Request>> pending_decodes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_
