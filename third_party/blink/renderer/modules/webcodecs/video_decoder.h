// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/video_decoder.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class ScriptState;
class ReadableStream;
class VideoDecoderInitParameters;
class WritableStream;
class Visitor;

class MODULES_EXPORT VideoDecoder final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoDecoder* Create(ScriptState*, ExceptionState&);
  VideoDecoder(ScriptState*, ExceptionState&);
  ~VideoDecoder() override;

  // video_decoder.idl implementation.
  ReadableStream* readable() const;
  WritableStream* writable() const;
  ScriptPromise initialize(const VideoDecoderInitParameters*, ExceptionState&);

  // GarbageCollected override.
  void Trace(Visitor*) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Helper classes that manage the input and output streams.
  class WritableSink;
  class ReadableSource;

  // Creates a new |write_resolver_| and returns a promise from it.
  ScriptPromise CreateWritePromise();

  // Resolves |write_resolver_| if the current state can accept a write.
  void MaybeAcceptWrite();

  // Rejects promises and shuts down streams.
  void HandleError();

  // Called by |decoder_|.
  void OnInitializeDone(bool success);
  void OnDecodeDone(media::DecodeStatus);
  void OnOutput(scoped_refptr<media::VideoFrame>);

  // Called by WritableSink.
  ScriptPromise Start(ExceptionState&);
  ScriptPromise Write(ScriptValue chunk, ExceptionState&);
  ScriptPromise Close(ExceptionState&);
  ScriptPromise Abort(ExceptionState&);

  // Called by ReadableSource.
  ScriptPromise Pull();
  ScriptPromise Cancel();

  Member<ScriptState> script_state_;
  Member<WritableStream> writable_;
  Member<ReadableStream> readable_;
  Member<ReadableSource> readable_source_;

  // Signals completion of initialize().
  Member<ScriptPromiseResolver> initialize_resolver_;

  // Signals ability to accept an input chunk.
  Member<ScriptPromiseResolver> write_resolver_;

  std::unique_ptr<media::VideoDecoder> decoder_;
  bool has_error_ = false;
  bool initialized_ = false;
  int pending_decodes_ = 0;

  base::WeakPtr<VideoDecoder> weak_this_;
  base::WeakPtrFactory<VideoDecoder> weak_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_
