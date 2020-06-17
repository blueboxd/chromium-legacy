// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CHROME_SPEECH_RECOGNITION_CLIENT_H_
#define CHROME_RENDERER_MEDIA_CHROME_SPEECH_RECOGNITION_CLIENT_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "chrome/common/caption.mojom.h"
#include "media/base/audio_buffer.h"
#include "media/base/speech_recognition_client.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace media {
class ChannelMixer;
}  // namespace media

class ChromeSpeechRecognitionClient
    : public media::SpeechRecognitionClient,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  explicit ChromeSpeechRecognitionClient(content::RenderFrame* render_frame);
  ChromeSpeechRecognitionClient(const ChromeSpeechRecognitionClient&) = delete;
  ChromeSpeechRecognitionClient& operator=(
      const ChromeSpeechRecognitionClient&) = delete;
  ~ChromeSpeechRecognitionClient() override;

  // media::SpeechRecognitionClient
  void AddAudio(scoped_refptr<media::AudioBuffer> buffer) override;
  bool IsSpeechRecognitionAvailable() override;

  // Callback executed when the recognizer is bound. Sets the flag indicating
  // whether the speech recognition service supports multichannel audio.
  void OnRecognizerBound(bool is_multichannel_supported);

  // media::mojom::SpeechRecognitionRecognizerClient
  void OnSpeechRecognitionRecognitionEvent(
      media::mojom::SpeechRecognitionResultPtr result) override;

 private:
  media::mojom::AudioDataS16Ptr ConvertToAudioDataS16(
      scoped_refptr<media::AudioBuffer> buffer);

  // Called as a response to sending a transcription to the browser.
  void OnTranscriptionCallback(bool success);
  // Recreates the temporary audio bus if the frame count or channel count
  // changed and reads the frames from the buffer into the temporary audio bus.
  void CopyBufferToTempAudioBus(const media::AudioBuffer& buffer);

  // Resets the temporary monaural audio bus and the channel mixer used to
  // combine multiple audio channels.
  void ResetChannelMixer(const media::AudioBuffer& buffer);
  bool IsUrlBlocked(const std::string& url) const;

  mojo::Remote<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_;
  mojo::Remote<media::mojom::SpeechRecognitionRecognizer>
      speech_recognition_recognizer_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognition_client_receiver_{this};
  mojo::Remote<chrome::mojom::CaptionHost> caption_host_;

  bool is_website_blocked_ = false;
  const base::flat_set<std::string> blocked_urls_;

  // The temporary audio bus used to convert the raw audio to the appropriate
  // format.
  std::unique_ptr<media::AudioBus> temp_audio_bus_;

  // Whether the browser is still requesting transcriptions.
  bool is_browser_requesting_transcription_ = true;
  // The temporary audio bus used to mix multichannel audio into a single
  // channel.
  std::unique_ptr<media::AudioBus> monaural_audio_bus_;

  std::unique_ptr<media::ChannelMixer> channel_mixer_;

  // The layout used to instantiate the channel mixer.
  media::ChannelLayout channel_layout_ =
      media::ChannelLayout::CHANNEL_LAYOUT_NONE;

  // A flag indicating whether the speech recognition service supports
  // multichannel audio.
  bool is_multichannel_supported_ = false;
};

#endif  // CHROME_RENDERER_MEDIA_CHROME_SPEECH_RECOGNITION_CLIENT_H_
