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
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_codecs_error_callback.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/decoder_template.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace media {

class VideoFrame;
class DecoderBuffer;
class MediaLog;

}  // namespace media

namespace blink {

class EncodedVideoChunk;
class EncodedVideoConfig;
class ExceptionState;
class VideoDecoderInit;
class VideoFrame;
class V8VideoFrameOutputCallback;

class MODULES_EXPORT VideoDecoderTraits {
 public:
  using InitType = VideoDecoderInit;
  using OutputType = VideoFrame;
  using MediaOutputType = media::VideoFrame;
  using MediaDecoderType = media::VideoDecoder;
  using OutputCallbackType = V8VideoFrameOutputCallback;
  using ConfigType = EncodedVideoConfig;
  using InputType = EncodedVideoChunk;

  static std::unique_ptr<MediaDecoderType> CreateDecoder(
      ExecutionContext& execution_context,
      media::MediaLog* media_log);
  static void InitializeDecoder(MediaDecoderType& decoder,
                                const ConfigType& config,
                                MediaDecoderType::InitCB init_cb,
                                MediaDecoderType::OutputCB output_cb);
  static int GetMaxDecodeRequests(const MediaDecoderType& decoder);
  static scoped_refptr<media::DecoderBuffer> MakeDecoderBuffer(
      const InputType& input);
};

class MODULES_EXPORT VideoDecoder : public DecoderTemplate<VideoDecoderTraits> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoDecoder* Create(ScriptState*,
                              const VideoDecoderInit*,
                              ExceptionState&);

  VideoDecoder(ScriptState*, const VideoDecoderInit*, ExceptionState&);
  ~VideoDecoder() override = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_
