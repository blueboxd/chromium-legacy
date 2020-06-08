// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_
#define CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_

#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "library_loaders/libchrometts.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace tts {

class TtsService : public mojom::TtsService, public mojom::TtsStream {
 public:
  explicit TtsService(mojo::PendingReceiver<mojom::TtsService> receiver);
  ~TtsService() override;

 private:
  // TtsService:
  void BindTtsStream(mojo::PendingReceiver<mojom::TtsStream> receiver) override;

  // TtsStream:
  void InstallVoice(const std::string& voice_name,
                    const std::vector<uint8_t>& voice_bytes,
                    InstallVoiceCallback callback) override;
  void SelectVoice(const std::string& voice_name,
                   SelectVoiceCallback callback) override;
  void Init(const std::vector<uint8_t>& text_jspb,
            InitCallback callback) override;
  void Read(ReadCallback callback) override;
  void Finalize() override;

  LibChromeTtsLoader libchrometts_;
  mojo::Receiver<mojom::TtsService> service_receiver_;
  mojo::Receiver<mojom::TtsStream> stream_receiver_;
};

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_
