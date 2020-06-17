// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_speech_recognition_client.h"

#include <utility>

#include "base/metrics/field_trial_params.h"
#include "content/public/renderer/render_frame.h"
#include "media/base/channel_mixer.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"

// Get the list of blocked URLs defined by the Finch experiment parameter. These
// websites provide captions by default and thus do not require the live caption
// feature.
std::vector<std::string> GetBlockedURLs() {
  return base::SplitString(base::GetFieldTrialParamValueByFeature(
                               media::kLiveCaption, "blocked_websites"),
                           ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

ChromeSpeechRecognitionClient::ChromeSpeechRecognitionClient(
    content::RenderFrame* render_frame)
    : blocked_urls_(GetBlockedURLs()) {
  mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_receiver =
          speech_recognition_context_.BindNewPipeAndPassReceiver();
  speech_recognition_context_->BindRecognizer(
      speech_recognition_recognizer_.BindNewPipeAndPassReceiver(),
      speech_recognition_client_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&ChromeSpeechRecognitionClient::OnRecognizerBound,
                     base::Unretained(this)));
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      std::move(speech_recognition_context_receiver));
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      caption_host_.BindNewPipeAndPassReceiver());
  is_website_blocked_ = IsUrlBlocked(
      render_frame->GetWebFrame()->GetSecurityOrigin().ToString().Utf8());
}

void ChromeSpeechRecognitionClient::OnRecognizerBound(
    bool is_multichannel_supported) {
  is_multichannel_supported_ = is_multichannel_supported;
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
  return !is_website_blocked_ && is_browser_requesting_transcription_ &&
         speech_recognition_recognizer_.is_bound() &&
         speech_recognition_recognizer_.is_connected();
}

void ChromeSpeechRecognitionClient::OnSpeechRecognitionRecognitionEvent(
    media::mojom::SpeechRecognitionResultPtr result) {
  caption_host_->OnTranscription(
      chrome::mojom::TranscriptionResult::New(result->transcription,
                                              result->is_final),
      base::BindOnce(&ChromeSpeechRecognitionClient::OnTranscriptionCallback,
                     base::Unretained(this)));
}

void ChromeSpeechRecognitionClient::OnTranscriptionCallback(bool success) {
  is_browser_requesting_transcription_ = success;
}

void ChromeSpeechRecognitionClient::CopyBufferToTempAudioBus(
    const media::AudioBuffer& buffer) {
  if (!temp_audio_bus_ ||
      buffer.channel_count() != temp_audio_bus_->channels() ||
      buffer.frame_count() != temp_audio_bus_->frames()) {
    temp_audio_bus_ =
        media::AudioBus::Create(buffer.channel_count(), buffer.frame_count());
  }

  buffer.ReadFrames(buffer.frame_count(),
                    /* source_frame_offset */ 0, /* dest_frame_offset */ 0,
                    temp_audio_bus_.get());
}

void ChromeSpeechRecognitionClient::ResetChannelMixer(
    const media::AudioBuffer& buffer) {
  if (!monaural_audio_bus_ ||
      buffer.frame_count() != monaural_audio_bus_->frames()) {
    monaural_audio_bus_ =
        media::AudioBus::Create(1 /* channels */, buffer.frame_count());
  }

  if (buffer.channel_layout() != channel_layout_) {
    channel_layout_ = buffer.channel_layout();
    channel_mixer_ = std::make_unique<media::ChannelMixer>(
        buffer.channel_layout(), media::CHANNEL_LAYOUT_MONO);
  }
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

  // If multichannel audio is not supported by the speech recognition service,
  // mix the channels into a monaural channel before converting it.
  if (buffer->channel_count() > 1 && !is_multichannel_supported_) {
    signed_buffer->channel_count = 1;
    CopyBufferToTempAudioBus(*buffer);
    ResetChannelMixer(*buffer);
    signed_buffer->data.resize(buffer->frame_count());
    channel_mixer_->Transform(temp_audio_bus_.get(), monaural_audio_bus_.get());
    monaural_audio_bus_->ToInterleaved<media::SignedInt16SampleTypeTraits>(
        monaural_audio_bus_->frames(), &signed_buffer->data[0]);
    return signed_buffer;
  }

  // If the audio is already in the interleaved signed int 16 format, directly
  // assign it to the buffer.
  if (buffer->sample_format() == media::SampleFormat::kSampleFormatS16) {
    int16_t* audio_data = reinterpret_cast<int16_t*>(buffer->channel_data()[0]);
    signed_buffer->data.assign(
        audio_data,
        audio_data + buffer->frame_count() * buffer->channel_count());
    return signed_buffer;
  }

  // Convert the raw audio to the interleaved signed int 16 sample type.
  CopyBufferToTempAudioBus(*buffer);
  signed_buffer->data.resize(buffer->frame_count() * buffer->channel_count());
  temp_audio_bus_->ToInterleaved<media::SignedInt16SampleTypeTraits>(
      temp_audio_bus_->frames(), &signed_buffer->data[0]);

  return signed_buffer;
}

bool ChromeSpeechRecognitionClient::IsUrlBlocked(const std::string& url) const {
  return blocked_urls_.find(url) != blocked_urls_.end();
}
