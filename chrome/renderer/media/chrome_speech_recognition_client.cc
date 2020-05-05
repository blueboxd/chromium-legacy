// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_speech_recognition_client.h"

#include <utility>

#include "content/public/renderer/render_frame.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

ChromeSpeechRecognitionClient::ChromeSpeechRecognitionClient(
    content::RenderFrame* render_frame) {
  mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_receiver =
          speech_recognition_context_.BindNewPipeAndPassReceiver();
  speech_recognition_context_->BindRecognizer(
      speech_recognition_recognizer_.BindNewPipeAndPassReceiver(),
      speech_recognition_client_receiver_.BindNewPipeAndPassRemote());
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      std::move(speech_recognition_context_receiver));
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      caption_host_.BindNewPipeAndPassReceiver());
}

ChromeSpeechRecognitionClient::~ChromeSpeechRecognitionClient() = default;

void ChromeSpeechRecognitionClient::AddAudio(
    scoped_refptr<media::AudioBuffer> buffer) {
  DCHECK(buffer);
  if (IsSpeechRecognitionAvailable()) {
    speech_recognition_recognizer_->SendAudioToSpeechRecognitionService(
        ConvertToAudioDataS16(std::move(buffer)));
  }
}

bool ChromeSpeechRecognitionClient::IsSpeechRecognitionAvailable() {
  return speech_recognition_recognizer_.is_bound() &&
         speech_recognition_recognizer_.is_connected();
}

void ChromeSpeechRecognitionClient::OnSpeechRecognitionRecognitionEvent(
    const std::string& transcription) {
  // This is a heuristic for whether the transcription is partial or final.
  // If the previous transcription is much longer than the current one, send
  // the previous transcription as a final result. Then, send the current one.
  // TODO(1055150): Get whether the transcription is partial or final from the
  // speech service.
  if ((transcription.size() < previous_transcription.size() / 2) &&
      (transcription.size() < previous_transcription.size() - 10)) {
    caption_host_->OnTranscription(
        chrome::mojom::TranscriptionResult::New(previous_transcription, true));
  }
  caption_host_->OnTranscription(
      chrome::mojom::TranscriptionResult::New(transcription, false));
  previous_transcription = transcription;
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
