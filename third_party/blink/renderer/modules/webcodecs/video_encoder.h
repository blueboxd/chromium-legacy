// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_codecs_error_callback.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class VideoEncoderInit;
class VideoEncoderTuneOptions;
class VideoEncoderEncodeOptions;
class Visitor;

class MODULES_EXPORT VideoEncoder final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoEncoder* Create(ScriptState*, VideoEncoderInit*, ExceptionState&);
  VideoEncoder(ScriptState*, VideoEncoderInit*, ExceptionState&);
  ~VideoEncoder() override;

  // video_encoder.idl implementation.
  ScriptPromise tune(const VideoEncoderTuneOptions* params, ExceptionState&);
  ScriptPromise encode(const VideoFrame* frame,
                       const VideoEncoderEncodeOptions*,
                       ExceptionState&);
  ScriptPromise close();

  // GarbageCollected override.
  void Trace(Visitor*) override;

 private:
  void DoEncoding(const VideoFrame* frame, bool force_keyframe);

  void CallOutputCallback(EncodedVideoChunk* chunk);

  gfx::Size frame_size_;

  Member<ScriptState> script_state_;

  Member<V8VideoEncoderOutputCallback> output_callback_;
  Member<V8WebCodecsErrorCallback> error_callback_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_
