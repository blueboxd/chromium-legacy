// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_browser_interface_binders.h"

#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/browser/media_resource_provider_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

void PopulateFuchsiaFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    MediaResourceProviderService* media_resource_provider_service) {
  map->Add<media::mojom::FuchsiaMediaResourceProvider>(
      base::BindRepeating(&MediaResourceProviderService::Bind,
                          base::Unretained(media_resource_provider_service)));
}
