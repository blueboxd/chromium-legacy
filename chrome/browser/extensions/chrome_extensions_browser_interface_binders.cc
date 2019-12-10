// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_interface_binders.h"

#include "base/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/media/router/media_router_feature.h"       // nogncheck
#include "chrome/browser/media/router/mojo/media_router_desktop.h"  // nogncheck
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

#if defined(OS_CHROMEOS)
#include "chromeos/services/media_perception/public/mojom/media_perception.mojom.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/media_perception_private/media_perception_api_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#endif
#endif

namespace extensions {

namespace {
#if defined(OS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Resolves InputEngineManager receiver in InputMethodManager.
void BindInputEngineManager(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chromeos::ime::mojom::InputEngineManager> receiver) {
  chromeos::input_method::InputMethodManager::Get()->ConnectInputEngineManager(
      std::move(receiver));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#endif
}  // namespace

void PopulateChromeFrameBindersForExtension(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>*
        binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  DCHECK(extension);
  auto* context = render_frame_host->GetProcess()->GetBrowserContext();
  if (media_router::MediaRouterEnabled(context) &&
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kMediaRouterPrivate)) {
    binder_map->Add<media_router::mojom::MediaRouter>(
        base::BindRepeating(&media_router::MediaRouterDesktop::BindToReceiver,
                            base::RetainedRef(extension), context));
  }

#if defined(OS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Registry InputEngineManager for official Google XKB Input only.
  if (extension->id() == chromeos::extension_ime_util::kXkbExtensionId) {
    binder_map->Add<chromeos::ime::mojom::InputEngineManager>(
        base::BindRepeating(&BindInputEngineManager));
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  if (extension->permissions_data()->HasAPIPermission(
          APIPermission::kMediaPerceptionPrivate)) {
    extensions::ExtensionsAPIClient* client =
        extensions::ExtensionsAPIClient::Get();
    extensions::MediaPerceptionAPIDelegate* delegate = nullptr;
    if (client)
      delegate = client->GetMediaPerceptionAPIDelegate();
    if (delegate) {
      // Note that it is safe to use base::Unretained here because |delegate| is
      // owned by the |client|, which is instantiated by the
      // ChromeExtensionsBrowserClient, which in turn is owned and lives as long
      // as the BrowserProcessImpl.
      binder_map->Add<chromeos::media_perception::mojom::MediaPerception>(
          base::BindRepeating(&extensions::MediaPerceptionAPIDelegate::
                                  ForwardMediaPerceptionReceiver,
                              base::Unretained(delegate)));
    }
  }
#endif
}

}  // namespace extensions
