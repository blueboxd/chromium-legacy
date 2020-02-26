// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_INTERFACE_BINDERS_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_INTERFACE_BINDERS_H_

#include "services/service_manager/public/cpp/binder_map.h"

namespace content {
class RenderFrameHost;
}  // namespace content

class MediaResourceProviderService;

// PopulateFuchsiaFrameBinders() registers BrowserInterfaceBroker's
// GetInterface() handler callbacks for Fuchsia-specific RenferFrame-scoped
// interfaces. This mechanism will replace interface registries and binders used
// for handling InterfaceProvider's GetInterface() calls (see crbug.com/718652).
void PopulateFuchsiaFrameBinders(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>* map,
    MediaResourceProviderService* media_resource_provider_service);

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_INTERFACE_BINDERS_H_
