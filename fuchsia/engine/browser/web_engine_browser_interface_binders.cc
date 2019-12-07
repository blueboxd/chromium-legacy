// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_browser_interface_binders.h"

#include "fuchsia/engine/browser/audio_consumer_provider_service.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/browser/web_engine_cdm_service.h"
#include "media/fuchsia/mojom/fuchsia_cdm_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace {

void BindAudioConsumerProvider(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaAudioConsumerProvider>
        receiver) {
  FrameImpl* frame_impl = FrameImpl::FromRenderFrameHost(frame_host);
  if (frame_impl)
    frame_impl->BindAudioConsumerProvider(std::move(receiver));
}

}  // namespace

void PopulateFuchsiaFrameBinders(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>* map,
    WebEngineCdmService* cdm_service) {
  map->Add<media::mojom::FuchsiaCdmProvider>(
      base::BindRepeating(&WebEngineCdmService::BindFuchsiaCdmProvider,
                          base::Unretained(cdm_service)));

  map->Add<media::mojom::FuchsiaAudioConsumerProvider>(
      base::BindRepeating(&BindAudioConsumerProvider));
}
