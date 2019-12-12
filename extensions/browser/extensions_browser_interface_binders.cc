// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_browser_interface_binders.h"

#include <string>

#include "base/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/mojo/keep_alive_impl.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/mojom/keep_alive.mojom.h"  // nogncheck

#if BUILDFLAG(ENABLE_WIFI_DISPLAY)
#include "extensions/browser/api/display_source/wifi_display/wifi_display_media_service_impl.h"
#include "extensions/browser/api/display_source/wifi_display/wifi_display_session_service_impl.h"
#endif

namespace extensions {
namespace {

#if BUILDFLAG(ENABLE_WIFI_DISPLAY)
bool ExtensionHasPermission(const Extension* extension,
                            content::RenderProcessHost* render_process_host,
                            const std::string& permission_name) {
  Feature::Context context =
      ProcessMap::Get(render_process_host->GetBrowserContext())
          ->GetMostLikelyContextType(extension, render_process_host->GetID());

  return ExtensionAPI::GetSharedInstance()
      ->IsAvailable(permission_name, extension, context, extension->url())
      .is_available();
}
#endif

}  // namespace

void PopulateExtensionFrameBinders(service_manager::BinderMapWithContext<
                                       content::RenderFrameHost*>* binder_map,
                                   content::RenderFrameHost* render_frame_host,
                                   const Extension* extension) {
  DCHECK(extension);

  auto* context = render_frame_host->GetProcess()->GetBrowserContext();
  binder_map->Add<KeepAlive>(base::BindRepeating(
      &KeepAliveImpl::Create, context, base::RetainedRef(extension)));

#if BUILDFLAG(ENABLE_WIFI_DISPLAY)
  if (ExtensionHasPermission(extension, render_frame_host->GetProcess(),
                             "displaySource")) {
    binder_map->Add<mojom::WiFiDisplaySessionService>(
        base::BindRepeating(&WiFiDisplaySessionServiceImpl::BindToReceiver,
                            context);
    binder_map->Add<mojom::WiFiDisplayMediaService>(
        base::BindRepeating(&WiFiDisplayMediaServiceImpl::BindToReceiver,
                            context);
  }
#endif
}

}  // namespace extensions
