// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CHROME_SPEECH_RECOGNITION_CLIENT_H_
#define CHROME_RENDERER_MEDIA_CHROME_SPEECH_RECOGNITION_CLIENT_H_

#include <memory>
#include <string>

#include "chrome/common/caption.mojom.h"
#include "media/base/audio_buffer.h"
#include "media/base/speech_recognition_client.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrame;
}  // namespace content

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

  // media::mojom::SpeechRecognitionRecognizerClient
  void OnSpeechRecognitionRecognitionEvent(
      const std::string& transcription) override;

 private:
  media::mojom::AudioDataS16Ptr ConvertToAudioDataS16(
      scoped_refptr<media::AudioBuffer> buffer);

  mojo::Remote<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_;
  mojo::Remote<media::mojom::SpeechRecognitionRecognizer>
      speech_recognition_recognizer_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognition_client_receiver_{this};
  mojo::Remote<chrome::mojom::CaptionHost> caption_host_;

  // The temporary audio bus used to convert the raw audio to the appropriate
  // format.
  std::unique_ptr<media::AudioBus> temp_audio_bus_;

  // The previous transcription received from the service. This is used for a
  // temporary heuristic to determine whether a transcription is partil or
  // final, and will be removed once the service itself sends partial or final
  // information.
  std::string previous_transcription;
};

#endif  // CHROME_RENDERER_MEDIA_CHROME_SPEECH_RECOGNITION_CLIENT_H_
