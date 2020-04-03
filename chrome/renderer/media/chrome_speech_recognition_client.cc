// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_speech_recognition_client.h"

#include "content/public/renderer/render_frame.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

ChromeSpeechRecognitionClient::ChromeSpeechRecognitionClient(
    content::RenderFrame* render_frame) {
  mojo::PendingReceiver<media::mojom::SodaContext> soda_context_receiver =
      soda_context_.BindNewPipeAndPassReceiver();
  soda_context_->BindRecognizer(
      soda_recognizer_.BindNewPipeAndPassReceiver(),
      soda_recognition_client_receiver_.BindNewPipeAndPassRemote());
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      std::move(soda_context_receiver));
}

ChromeSpeechRecognitionClient::~ChromeSpeechRecognitionClient() = default;

void ChromeSpeechRecognitionClient::AddAudio(
    scoped_refptr<media::AudioBuffer> buffer) {
  DCHECK(buffer);
  if (IsSpeechRecognitionAvailable()) {
    soda_recognizer_->SendAudioToSoda(ConvertToAudioDataS16(std::move(buffer)));
  }
}

bool ChromeSpeechRecognitionClient::IsSpeechRecognitionAvailable() {
  return soda_recognizer_.is_bound() && soda_recognizer_.is_connected();
}

void ChromeSpeechRecognitionClient::OnSodaRecognitionEvent(
    const std::string& transcription) {
  // TODO(evliu): Pass the captions to the caption controller.
  NOTIMPLEMENTED();
}

media::mojom::AudioDataS16Ptr
ChromeSpeechRecognitionClient::ConvertToAudioDataS16(
    scoped_refptr<media::AudioBuffer> buffer) {
  DCHECK_GT(buffer->frame_count(), 0);
  DCHECK_GT(buffer->channel_count(), 0);
  DCHECK_GT(buffer->sample_rate(), 0);

  auto signed_buffer = media::mojom::AudioDataS16::New();
  signed_buffer->channel_count = buffer->channel_count();
  signed_buffer->frame_count = buffer->frame_count();
  signed_buffer->sample_rate = buffer->sample_rate();

  if (buffer->sample_format() == media::SampleFormat::kSampleFormatS16) {
    int16_t* audio_data = reinterpret_cast<int16_t*>(buffer->channel_data()[0]);
    signed_buffer->data.assign(
        audio_data,
        audio_data + buffer->frame_count() * buffer->channel_count());
    return signed_buffer;
  }

  // Convert the raw audio to the interleaved signed int 16 sample type.
  if (!temp_audio_bus_ ||
      buffer->channel_count() != temp_audio_bus_->channels() ||
      buffer->frame_count() != temp_audio_bus_->frames()) {
    temp_audio_bus_ =
        media::AudioBus::Create(buffer->channel_count(), buffer->frame_count());
  }

  buffer->ReadFrames(buffer->frame_count(),
                     /* source_frame_offset */ 0, /* dest_frame_offset */ 0,
                     temp_audio_bus_.get());

  signed_buffer->data.resize(buffer->frame_count() * buffer->channel_count());
  temp_audio_bus_->ToInterleaved<media::SignedInt16SampleTypeTraits>(
      temp_audio_bus_->frames(), &signed_buffer->data[0]);

  return signed_buffer;
}
