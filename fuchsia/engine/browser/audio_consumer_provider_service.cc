// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/audio_consumer_provider_service.h"

#include <lib/fidl/cpp/interface_handle.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/default_context.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/frame_impl.h"

AudioConsumerProviderService::AudioConsumerProviderService() = default;
AudioConsumerProviderService::~AudioConsumerProviderService() = default;

void AudioConsumerProviderService::Bind(
    mojo::PendingReceiver<media::mojom::FuchsiaAudioConsumerProvider>
        receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void AudioConsumerProviderService::CreateAudioConsumer(
    fidl::InterfaceRequest<fuchsia::media::AudioConsumer> request) {
  auto factory = base::fuchsia::ComponentContextForCurrentProcess()
                     ->svc()
                     ->Connect<fuchsia::media::SessionAudioConsumerFactory>();
  factory->CreateAudioConsumer(session_id_, std::move(request));
}