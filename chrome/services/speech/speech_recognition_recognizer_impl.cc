// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_recognition_recognizer_impl.h"

#include "base/bind.h"
#include "components/soda/constants.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_sample_types.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/mojo/common/media_type_converters.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(ENABLE_SODA)
#include "chrome/services/soda/internal/soda_client.h"
#endif  // BUILDFLAG(ENABLE_SODA)

namespace speech {

namespace {

#if BUILDFLAG(ENABLE_SODA)
// Callback executed by the SODA library on a speech recognition event. The
// callback handle is a void pointer to the SpeechRecognitionRecognizerImpl that
// owns the SODA instance. SpeechRecognitionRecognizerImpl owns the SodaClient
// which owns the instance of SODA and their sequential destruction order
// ensures that this callback will never be called with an invalid callback
// handle to the SpeechRecognitionRecognizerImpl.
void RecognitionCallback(const char* result, void* callback_handle) {
  DCHECK(callback_handle);
  static_cast<SpeechRecognitionRecognizerImpl*>(callback_handle)
      ->recognition_event_callback()
      .Run(std::string(result));
}
#endif  // BUILDFLAG(ENABLE_SODA)

}  // namespace

SpeechRecognitionRecognizerImpl::~SpeechRecognitionRecognizerImpl() = default;

void SpeechRecognitionRecognizerImpl::Create(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
        remote) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new SpeechRecognitionRecognizerImpl(std::move(remote))),
      std::move(receiver));
}

void SpeechRecognitionRecognizerImpl::OnRecognitionEvent(
    const std::string& result) {
  client_remote_->OnSpeechRecognitionRecognitionEvent(result);
}

SpeechRecognitionRecognizerImpl::SpeechRecognitionRecognizerImpl(
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> remote)
    : client_remote_(std::move(remote)) {
  recognition_event_callback_ = media::BindToCurrentLoop(
      base::Bind(&SpeechRecognitionRecognizerImpl::OnRecognitionEvent,
                 weak_factory_.GetWeakPtr()));
#if BUILDFLAG(ENABLE_SODA)
  soda_client_ = std::make_unique<soda::SodaClient>(GetSodaBinaryPath());
#endif  // BUILDFLAG(ENABLE_SODA)
}

void SpeechRecognitionRecognizerImpl::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr buffer) {
  int channel_count = buffer->channel_count;
  int frame_count = buffer->frame_count;
  int sample_rate = buffer->sample_rate;
  int num_samples;
  int data_size;
  if (channel_count <= 0 || channel_count > media::limits::kMaxChannels ||
      sample_rate <= 0 || frame_count <= 0 ||
      !base::CheckMul(frame_count, channel_count).AssignIfValid(&num_samples) ||
      !base::CheckMul(num_samples, sizeof(int16_t)).AssignIfValid(&data_size)) {
    mojo::ReportBadMessage("Invalid audio data received.");
    return;
  }

#if BUILDFLAG(ENABLE_SODA)
  DCHECK(soda_client_);
  if (!soda_client_->IsInitialized() ||
      soda_client_->DidAudioPropertyChange(sample_rate, channel_count)) {
    // Initialize the SODA instance.
    auto config_file_path = GetSodaConfigPath().value();
    SodaConfig config;
    config.channel_count = channel_count;
    config.sample_rate = sample_rate;
    config.config_file = config_file_path.c_str();
    config.callback = RecognitionCallback;
    config.callback_handle = this;
    soda_client_->Reset(config);
  }

  soda_client_->AddAudio(reinterpret_cast<char*>(buffer->data.data()),
                         data_size);
#endif  // BUILDFLAG(ENABLE_SODA)
}

}  // namespace speech
