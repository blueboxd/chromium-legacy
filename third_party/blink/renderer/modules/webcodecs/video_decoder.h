// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class ScriptState;
class ReadableStream;
class VideoDecoderInitParameters;
class WritableStream;

class MODULES_EXPORT VideoDecoder final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoDecoder* Create();
  VideoDecoder();
  ScriptPromise Initialize(ScriptState*,
                           const VideoDecoderInitParameters*,
                           ExceptionState&);
  ScriptPromise Flush(ScriptState*, ExceptionState&);
  void Close();

  // video_decoder.idl implementation.
  ReadableStream* readable() const;
  WritableStream* writable() const;

  // GarbageCollected override.
  void Trace(Visitor*) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_
