// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_AUDIO_CONSUMER_PROVIDER_SERVICE_H_
#define FUCHSIA_ENGINE_BROWSER_AUDIO_CONSUMER_PROVIDER_SERVICE_H_

#include "media/fuchsia/mojom/fuchsia_audio_consumer_provider.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class AudioConsumerProviderService
    : public media::mojom::FuchsiaAudioConsumerProvider {
 public:
  AudioConsumerProviderService(const AudioConsumerProviderService&) = delete;
  AudioConsumerProviderService& operator=(const AudioConsumerProviderService&) =
      delete;

  AudioConsumerProviderService();
  ~AudioConsumerProviderService() final;

  void Bind(mojo::PendingReceiver<media::mojom::FuchsiaAudioConsumerProvider>
                receiver);

  void set_session_id(uint64_t session_id) { session_id_ = session_id; }

 private:
  // media::mojom::FuchsiaAudioConsumerProvider implementation..
  void CreateAudioConsumer(
      fidl::InterfaceRequest<fuchsia::media::AudioConsumer> request) final;

  uint64_t session_id_ = 0;

  mojo::ReceiverSet<media::mojom::FuchsiaAudioConsumerProvider> receiver_set_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_AUDIO_CONSUMER_PROVIDER_SERVICE_H_