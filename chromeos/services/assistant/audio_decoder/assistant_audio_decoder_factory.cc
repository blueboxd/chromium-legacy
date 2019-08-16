// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder_factory.h"

#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace chromeos {
namespace assistant {

AssistantAudioDecoderFactory::AssistantAudioDecoderFactory(
    mojo::PendingReceiver<mojom::AssistantAudioDecoderFactory> receiver)
    : receiver_(this, std::move(receiver)) {}

AssistantAudioDecoderFactory::~AssistantAudioDecoderFactory() = default;

void AssistantAudioDecoderFactory::CreateAssistantAudioDecoder(
    mojom::AssistantAudioDecoderRequest request,
    mojom::AssistantAudioDecoderClientPtr client,
    mojom::AssistantMediaDataSourcePtr data_source) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AssistantAudioDecoder>(std::move(client),
                                              std::move(data_source)),
      mojo::PendingReceiver<mojom::AssistantAudioDecoder>(std::move(request)));
}

}  // namespace assistant
}  // namespace chromeos
