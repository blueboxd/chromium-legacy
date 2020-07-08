// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"

#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/waiting.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_config.h"

#include <memory>
#include <vector>

namespace blink {

// static
std::unique_ptr<AudioDecoderTraits::MediaDecoderType>
AudioDecoderTraits::CreateDecoder(ExecutionContext& execution_context,
                                  media::MediaLog* media_log) {
  return std::make_unique<media::FFmpegAudioDecoder>(
      execution_context.GetTaskRunner(TaskType::kInternalMedia), media_log);
}

// static
void AudioDecoderTraits::InitializeDecoder(
    MediaDecoderType& decoder,
    const ConfigType& config,
    MediaDecoderType::InitCB init_cb,
    MediaDecoderType::OutputCB output_cb) {
  std::vector<uint8_t> extra_data;
  if (config.hasDescription()) {
    DOMArrayBuffer* buffer;
    if (config.description().IsArrayBuffer()) {
      buffer = config.description().GetAsArrayBuffer();
    } else {
      // TODO(sandersd): Can IsNull() be true?
      DCHECK(config.description().IsArrayBufferView());
      buffer = config.description().GetAsArrayBufferView()->buffer();
    }
    // TODO(sandersd): Is it possible to not have Data()?
    uint8_t* start = static_cast<uint8_t*>(buffer->Data());
    size_t size = buffer->ByteLengthAsSizeT();
    extra_data.assign(start, start + size);
  }

  // TODO(chcunningham): Convert the rest of blink config -> media config.
  auto media_config =
      media::AudioDecoderConfig(media::kCodecAAC, media::kSampleFormatPlanarF32,
                                media::CHANNEL_LAYOUT_STEREO, 48000, extra_data,
                                media::EncryptionScheme::kUnencrypted);
  decoder.Initialize(media_config, nullptr /* cdm_context */,
                     std::move(init_cb), output_cb, media::WaitingCB());
}

// static
int AudioDecoderTraits::GetMaxDecodeRequests(const MediaDecoderType& decoder) {
  return 1;
}

// static
scoped_refptr<media::DecoderBuffer> AudioDecoderTraits::MakeDecoderBuffer(
    const InputType& chunk) {
  auto decoder_buffer = media::DecoderBuffer::CopyFrom(
      static_cast<uint8_t*>(chunk.data()->Data()),
      chunk.data()->ByteLengthAsSizeT());
  decoder_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(chunk.timestamp()));
  decoder_buffer->set_is_key_frame(chunk.type() == "key");
  return decoder_buffer;
}

// static
AudioDecoder* AudioDecoder::Create(ScriptState* script_state,
                                   const AudioDecoderInit* init,
                                   ExceptionState& exception_state) {
  return MakeGarbageCollected<AudioDecoder>(script_state, init,
                                            exception_state);
}

AudioDecoder::AudioDecoder(ScriptState* script_state,
                           const AudioDecoderInit* init,
                           ExceptionState& exception_state)
    : DecoderTemplate<AudioDecoderTraits>(script_state, init, exception_state) {
}

}  // namespace blink
