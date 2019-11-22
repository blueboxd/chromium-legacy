// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_browser_interface_binders.h"

#include "fuchsia/engine/browser/web_engine_cdm_service.h"
#include "media/fuchsia/mojom/fuchsia_cdm_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

void PopulateFuchsiaFrameBinders(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>* map,
    WebEngineCdmService* cdm_service) {
  map->Add<::media::mojom::FuchsiaCdmProvider>(
      base::BindRepeating(&WebEngineCdmService::BindFuchsiaCdmProvider,
                          base::Unretained(cdm_service)));
}
