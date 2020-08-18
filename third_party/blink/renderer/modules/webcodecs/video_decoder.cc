// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/video_decoder.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_config.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_config_eval.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder_broker.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

// static
std::unique_ptr<VideoDecoderTraits::MediaDecoderType>
VideoDecoderTraits::CreateDecoder(ExecutionContext& execution_context,
                                  media::MediaLog* media_log) {
  return std::make_unique<VideoDecoderBroker>(
      execution_context, Platform::Current()->GetGpuFactories());
}

// static
CodecConfigEval VideoDecoderTraits::CreateMediaConfig(
    const ConfigType& config,
    MediaConfigType* out_media_config,
    String* out_console_message) {
  DCHECK(out_media_config);
  DCHECK(out_console_message);

  bool is_codec_ambiguous = true;
  media::VideoCodec codec = media::kUnknownVideoCodec;
  media::VideoCodecProfile profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
  media::VideoColorSpace color_space = media::VideoColorSpace::REC709();
  uint8_t level = 0;
  bool parse_succeeded = media::ParseVideoCodecString(
      "", config.codec().Utf8(), &is_codec_ambiguous, &codec, &profile, &level,
      &color_space);

  if (!parse_succeeded) {
    *out_console_message = "Failed to parse codec string.";
    return CodecConfigEval::kInvalid;
  }

  if (is_codec_ambiguous) {
    *out_console_message = "Codec string is ambiguous.";
    return CodecConfigEval::kInvalid;
  }

  if (!media::IsSupportedVideoType({codec, profile, level, color_space})) {
    *out_console_message = "Configuration is not supported.";
    return CodecConfigEval::kUnsupported;
  }

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

  // TODO(sandersd): Either remove sizes from VideoDecoderConfig (replace with
  // sample aspect) or parse the AvcC here to get the actual size.
  // For the moment, hard-code 720p to prefer hardware decoders.
  gfx::Size size = gfx::Size(1280, 720);

  out_media_config->Initialize(codec, profile,
                               media::VideoDecoderConfig::AlphaMode::kIsOpaque,
                               color_space, media::kNoTransformation, size,
                               gfx::Rect(gfx::Point(), size), size, extra_data,
                               media::EncryptionScheme::kUnencrypted);

  return CodecConfigEval::kSupported;
}

// static
void VideoDecoderTraits::InitializeDecoder(
    MediaDecoderType& decoder,
    const MediaConfigType& media_config,
    MediaDecoderType::InitCB init_cb,
    MediaDecoderType::OutputCB output_cb) {
  decoder.Initialize(media_config, false /* low_delay */,
                     nullptr /* cdm_context */, std::move(init_cb), output_cb,
                     media::WaitingCB());
}

// static
int VideoDecoderTraits::GetMaxDecodeRequests(const MediaDecoderType& decoder) {
  return decoder.GetMaxDecodeRequests();
}

// static
scoped_refptr<media::DecoderBuffer> VideoDecoderTraits::MakeDecoderBuffer(
    const InputType& chunk) {
  // Convert |chunk| to a DecoderBuffer.
  auto decoder_buffer = media::DecoderBuffer::CopyFrom(
      static_cast<uint8_t*>(chunk.data()->Data()),
      chunk.data()->ByteLengthAsSizeT());
  decoder_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(chunk.timestamp()));
  // TODO(sandersd): Use kUnknownTimestamp instead of 0?
  decoder_buffer->set_duration(
      base::TimeDelta::FromMicroseconds(chunk.duration().value_or(0)));
  decoder_buffer->set_is_key_frame(chunk.type() == "key");

  return decoder_buffer;
}

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
    : DecoderTemplate<VideoDecoderTraits>(script_state, init, exception_state) {
}

}  // namespace blink
