// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_interface_registration.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/media/router/media_router_feature.h"  // nogncheck
#include "chrome/browser/media/router/mojo/media_router_desktop.h"  // nogncheck
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "services/service_manager/public/cpp/binder_registry.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_features.h"
#include "chromeos/services/ime/public/mojom/constants.mojom.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/media_perception/public/mojom/media_perception.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/system_connector.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/media_perception_private/media_perception_api_delegate.h"
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#include "media/capture/video/chromeos/renderer_facing_cros_image_capture.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#endif

#if defined(KIOSK_NEXT)
#include "chrome/browser/chromeos/kiosk_next_home/kiosk_next_home_interface_broker_impl.h"
#include "chrome/browser/chromeos/kiosk_next_home/mojom/kiosk_next_home_interface_broker.mojom.h"  // nogncheck
#endif  // defined(KIOSK_NEXT)

namespace extensions {
namespace {
#if defined(OS_CHROMEOS)

#if defined(GOOGLE_CHROME_BUILD)
// Resolves InputEngineManager request in InputMethodManager.
void BindInputEngineManager(
    chromeos::ime::mojom::InputEngineManagerRequest request,
    content::RenderFrameHost* source) {
  chromeos::input_method::InputMethodManager::Get()->ConnectInputEngineManager(
      std::move(request));
}
#endif  // defined(GOOGLE_CHROME_BUILD)

#if defined(KIOSK_NEXT)
const char kKioskNextHomeInterfaceBrokerImplKey[] = "cros_kiosk_next_home_impl";

void BindKioskNextHomeInterfaceBrokerRequest(
    content::BrowserContext* context,
    chromeos::kiosk_next_home::mojom::KioskNextHomeInterfaceBrokerRequest
        request,
    content::RenderFrameHost* source) {
  auto* impl =
      static_cast<chromeos::kiosk_next_home::KioskNextHomeInterfaceBrokerImpl*>(
          context->GetUserData(kKioskNextHomeInterfaceBrokerImplKey));
  if (!impl) {
    auto new_impl = std::make_unique<
        chromeos::kiosk_next_home::KioskNextHomeInterfaceBrokerImpl>(context);
    impl = new_impl.get();
    context->SetUserData(kKioskNextHomeInterfaceBrokerImplKey,
                         std::move(new_impl));
  }
  impl->BindRequest(std::move(request));
}
#endif  // defined(KIOSK_NEXT)

// Translates the renderer-side source ID to video device id.
void TranslateVideoDeviceId(
    const std::string& salt,
    const url::Origin& origin,
    const std::string& source_id,
    base::OnceCallback<void(const base::Optional<std::string>&)> callback) {
  auto callback_on_io_thread = base::BindOnce(
      [](const std::string& salt, const url::Origin& origin,
         const std::string& source_id,
         base::OnceCallback<void(const base::Optional<std::string>&)>
             callback) {
        content::GetMediaDeviceIDForHMAC(
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, salt,
            std::move(origin), source_id, std::move(callback));
      },
      salt, std::move(origin), source_id, std::move(callback));
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           std::move(callback_on_io_thread));
}

// Binds CrosImageCaptureRequest to a proxy which translates the source id into
// video device id and then forward the request to video capture service.
void BindRendererFacingCrosImageCapture(
    cros::mojom::CrosImageCaptureRequest request,
    content::RenderFrameHost* source) {
  cros::mojom::CrosImageCapturePtr proxy_ptr;
  auto proxy_request = mojo::MakeRequest(&proxy_ptr);

  // Bind proxy request to video_capture service.
  content::GetSystemConnector()->BindInterface(
      video_capture::mojom::kServiceName, std::move(proxy_request));

  auto security_origin = source->GetLastCommittedOrigin();
  auto media_device_id_salt =
      source->GetProcess()->GetBrowserContext()->GetMediaDeviceIDSalt();

  auto mapping_callback =
      base::BindRepeating(&TranslateVideoDeviceId, media_device_id_salt,
                          std::move(security_origin));

  // Bind origin request to proxy implementation.
  auto api_proxy = std::make_unique<media::RendererFacingCrosImageCapture>(
      std::move(proxy_ptr), std::move(mapping_callback));
  mojo::MakeStrongBinding(std::move(api_proxy), std::move(request));
}
#endif

}  // namespace

void RegisterChromeInterfacesForExtension(
    service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
        registry,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  DCHECK(extension);
  content::BrowserContext* context =
      render_frame_host->GetProcess()->GetBrowserContext();
  if (media_router::MediaRouterEnabled(context) &&
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kMediaRouterPrivate)) {
    registry->AddInterface(
        base::Bind(&media_router::MediaRouterDesktop::BindToRequest,
                   base::RetainedRef(extension), context));
  }

#if defined(OS_CHROMEOS)

#if defined(GOOGLE_CHROME_BUILD)
  // Registry InputEngineManager for official Google XKB Input only.
  if (extension->id() == chromeos::extension_ime_util::kXkbExtensionId) {
    registry->AddInterface(base::BindRepeating(&BindInputEngineManager));
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

#if defined(KIOSK_NEXT)
  if (base::FeatureList::IsEnabled(ash::features::kKioskNextShell) &&
      extension->id() == extension_misc::kKioskNextHomeAppId) {
    registry->AddInterface(
        base::BindRepeating(&BindKioskNextHomeInterfaceBrokerRequest, context));
  }
#endif  // defined(KIOSK_NEXT)

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
      registry->AddInterface(
          base::BindRepeating(&extensions::MediaPerceptionAPIDelegate::
                                  ForwardMediaPerceptionRequest,
                              base::Unretained(delegate)));
    }
  }
  if (extension->id().compare(extension_misc::kChromeCameraAppId) == 0 ||
      extension->id().compare(extension_misc::kChromeCameraAppDevId) == 0) {
    registry->AddInterface(
        base::BindRepeating(&BindRendererFacingCrosImageCapture));
  }
#endif
}

}  // namespace extensions
