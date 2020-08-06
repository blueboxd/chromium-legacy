// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DECODER_BROKER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DECODER_BROKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/decode_status.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// Implementation detail of AudioDecoderBroker. Helps safely perform decoder
// tasks on the media thread.
class MediaAudioTaskWrapper;

// Client interface for MediaAudioTaskWrapper. Implementation detail of
// AudioDecoderBroker, but we need to define it here to implement it below.
class CrossThreadAudioDecoderClient {
 public:
  virtual void OnDecodeOutput(scoped_refptr<media::AudioBuffer> buffer) = 0;
};

// This class brokers the connection between WebCodecs and an underlying
// media::AudioDecoder. It abstracts away details of construction and selection
// of the media/ decoder. It also handles thread-hopping as required by
// underlying APIS.
//
// A new underlying decoder is selected anytime Initialize() is called.
// TODO(chcunningham): Elide re-selection if the config has not significantly
// changed.
//
// All API calls and callbacks must occur on the main thread.
class MODULES_EXPORT AudioDecoderBroker : public media::AudioDecoder,
                                          public CrossThreadAudioDecoderClient {
 public:
  static constexpr char kDefaultDisplayName[] = "EmptyWebCodecsAudioDecoder";

  struct DecoderDetails {
    std::string display_name;
    bool is_platform_decoder;
    bool needs_bitstream_conversion;
  };

  explicit AudioDecoderBroker(ExecutionContext& execution_context);
  ~AudioDecoderBroker() override;

  // Disallow copy and assign.
  AudioDecoderBroker(const AudioDecoderBroker&) = delete;
  AudioDecoderBroker& operator=(const AudioDecoderBroker&) = delete;

  // AudioDecoder implementation.
  std::string GetDisplayName() const override;
  bool IsPlatformDecoder() const override;
  void Initialize(const media::AudioDecoderConfig& config,
                  media::CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const media::WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<media::DecoderBuffer> buffer,
              DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  bool NeedsBitstreamConversion() const override;

 private:
  void OnInitialize(InitCB init_cb,
                    media::Status status,
                    base::Optional<DecoderDetails> details);
  void OnDecodeDone(DecodeCB decode_cb, media::DecodeStatus status);
  void OnReset(base::OnceClosure reset_cb);

  // MediaAudioTaskWrapper::CrossThreadAudioDecoderClient
  void OnDecodeOutput(scoped_refptr<media::AudioBuffer> buffer) override;

  // Task runner for running codec work (traditionally the media thread).
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // Owner of state and methods to be used on media_task_runner_;
  std::unique_ptr<MediaAudioTaskWrapper> media_tasks_;

  // Wrapper state for GetDisplayName(), IsPlatformDecoder() and others.
  base::Optional<DecoderDetails> decoder_details_;

  // OutputCB saved from last call to Initialize().
  OutputCB output_cb_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AudioDecoderBroker> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DECODER_BROKER_H_
