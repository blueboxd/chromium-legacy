// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_SPEECH_RECOGNITION_CLIENT_H_
#define CHROME_RENDERER_CHROME_SPEECH_RECOGNITION_CLIENT_H_

#include <memory>

#include "media/base/audio_buffer.h"
#include "media/base/speech_recognition_client.h"
#include "media/mojo/mojom/soda_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrame;
}  // namespace content

class ChromeSpeechRecognitionClient
    : public media::SpeechRecognitionClient,
      public media::mojom::SodaRecognizerClient {
 public:
  explicit ChromeSpeechRecognitionClient(content::RenderFrame* render_frame);
  ChromeSpeechRecognitionClient(const ChromeSpeechRecognitionClient&) = delete;
  ChromeSpeechRecognitionClient& operator=(
      const ChromeSpeechRecognitionClient&) = delete;
  ~ChromeSpeechRecognitionClient() override;

  // media::SpeechRecognitionClient
  void AddAudio(scoped_refptr<media::AudioBuffer> buffer) override;
  bool IsSpeechRecognitionAvailable() override;

  // media::mojom::SodaRecognizerClient
  void OnSodaRecognitionEvent(const std::string& transcription) override;

 private:
  media::mojom::AudioDataS16Ptr ConvertToAudioDataS16(
      scoped_refptr<media::AudioBuffer> buffer);

  mojo::Remote<media::mojom::SodaContext> soda_context_;
  mojo::Remote<media::mojom::SodaRecognizer> soda_recognizer_;
  mojo::Receiver<media::mojom::SodaRecognizerClient>
      soda_recognition_client_receiver_{this};

  // The temporary audio bus used to convert the raw audio to the appropriate
  // format.
  std::unique_ptr<media::AudioBus> temp_audio_bus_;
};

#endif  // CHROME_RENDERER_CHROME_SPEECH_RECOGNITION_CLIENT_H_
