// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DECODER_H_

#include <stdint.h>
#include <memory>

#include "media/base/audio_decoder.h"
#include "media/base/media_log.h"
#include "media/base/status.h"
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

class AudioBuffer;
class DecoderBuffer;
class MediaLog;

}  // namespace media

namespace blink {

class AudioFrame;
class EncodedAudioChunk;
class EncodedAudioConfig;
class ExceptionState;
class AudioDecoderInit;
class V8AudioFrameOutputCallback;

class MODULES_EXPORT AudioDecoderTraits {
 public:
  using InitType = AudioDecoderInit;
  using OutputType = AudioFrame;
  using MediaOutputType = media::AudioBuffer;
  using MediaDecoderType = media::AudioDecoder;
  using OutputCallbackType = V8AudioFrameOutputCallback;
  using ConfigType = EncodedAudioConfig;
  using InputType = EncodedAudioChunk;

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

class MODULES_EXPORT AudioDecoder : public DecoderTemplate<AudioDecoderTraits> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioDecoder* Create(ScriptState*,
                              const AudioDecoderInit*,
                              ExceptionState&);

  AudioDecoder(ScriptState*, const AudioDecoderInit*, ExceptionState&);
  ~AudioDecoder() override = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DECODER_H_
