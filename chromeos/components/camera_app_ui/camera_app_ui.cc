// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/camera_app_ui/camera_app_ui.h"

#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "chromeos/components/camera_app_ui/camera_app_helper_impl.h"
#include "chromeos/components/camera_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_camera_app_resources.h"
#include "chromeos/grit/chromeos_camera_app_resources_map.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "media/capture/video/chromeos/camera_app_device_provider_impl.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/aura/window.h"
#include "ui/webui/webui_allowlist.h"

namespace chromeos {

namespace {

const struct {
  const char* path;
  int id;
} kGritResourceMap[] = {
    {"src/js/browser_proxy/browser_proxy.js",
     IDR_CAMERA_WEBUI_BROWSER_PROXY_JS},
    {"src/js/mojo/camera_intent.mojom-lite.js",
     IDR_CAMERA_CAMERA_INTENT_MOJOM_LITE_JS},
    {"src/js/mojo/image_capture.mojom-lite.js",
     IDR_CAMERA_IMAGE_CAPTURE_MOJOM_LITE_JS},
    {"src/js/mojo/camera_common.mojom-lite.js",
     IDR_CAMERA_CAMERA_COMMON_MOJOM_LITE_JS},
    {"src/js/mojo/camera_metadata.mojom-lite.js",
     IDR_CAMERA_CAMERA_METADATA_MOJOM_LITE_JS},
    {"src/js/mojo/camera_metadata_tags.mojom-lite.js",
     IDR_CAMERA_CAMERA_METADATA_TAGS_MOJOM_LITE_JS},
    {"src/js/mojo/camera_app.mojom-lite.js",
     IDR_CAMERA_CAMERA_APP_MOJOM_LITE_JS},
    {"src/js/mojo/mojo_bindings_lite.js", IDR_MOJO_MOJO_BINDINGS_LITE_JS},
    {"src/js/mojo/camera_app_helper.mojom-lite.js",
     IDR_CAMERA_CAMERA_APP_HELPER_MOJOM_LITE_JS},
    {"src/js/mojo/time.mojom-lite.js", IDR_CAMERA_TIME_MOJOM_LITE_JS},
    {"src/js/mojo/idle_manager.mojom-lite.js",
     IDR_CAMERA_IDLE_MANAGER_MOJOM_LITE_JS},
    {"src/js/mojo/camera_app.mojom-lite.js",
     IDR_CAMERA_CAMERA_APP_MOJOM_LITE_JS},
    {"src/js/mojo/geometry.mojom-lite.js", IDR_CAMERA_GEOMETRY_MOJOM_LITE_JS},
    {"src/js/mojo/range.mojom-lite.js", IDR_CAMERA_RANGE_MOJOM_LITE_JS},
};

content::WebUIDataSource* CreateCameraAppUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUICameraAppHost);

  // Add all settings resources.
  for (size_t i = 0; i < kChromeosCameraAppResourcesSize; i++) {
    source->AddResourcePath(kChromeosCameraAppResources[i].name,
                            kChromeosCameraAppResources[i].value);
  }

  for (const auto& res : kGritResourceMap) {
    source->AddResourcePath(res.path, res.id);
  }

  source->UseStringsJs();

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      std::string("worker-src 'self';"));

  return source;
}

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
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(callback_on_io_thread));
}

void HandleCameraResult(
    content::BrowserContext* context,
    uint32_t intent_id,
    arc::mojom::CameraIntentAction action,
    const std::vector<uint8_t>& data,
    chromeos_camera::mojom::CameraAppHelper::HandleCameraResultCallback
        callback) {
  auto* intent_helper =
      arc::ArcIntentHelperBridge::GetForBrowserContext(context);
  intent_helper->HandleCameraResult(intent_id, action, data,
                                    std::move(callback));
}

std::unique_ptr<media::CameraAppDeviceProviderImpl>
CreateCameraAppDeviceProvider(const url::Origin& security_origin,
                              content::BrowserContext* context) {
  auto media_device_id_salt = context->GetMediaDeviceIDSalt();

  mojo::PendingRemote<cros::mojom::CameraAppDeviceBridge> device_bridge;
  auto device_bridge_receiver = device_bridge.InitWithNewPipeAndPassReceiver();

  // Connects to CameraAppDeviceBridge from video_capture service.
  content::GetVideoCaptureService().ConnectToCameraAppDeviceBridge(
      std::move(device_bridge_receiver));

  auto mapping_callback =
      base::BindRepeating(&TranslateVideoDeviceId, media_device_id_salt,
                          std::move(security_origin));

  return std::make_unique<media::CameraAppDeviceProviderImpl>(
      std::move(device_bridge), std::move(mapping_callback));
}

std::unique_ptr<chromeos_camera::CameraAppHelperImpl> CreateCameraAppHelper(
    content::BrowserContext* browser_context,
    aura::Window* window) {
  DCHECK_NE(window, nullptr);
  auto handle_result_callback =
      base::BindRepeating(&HandleCameraResult, browser_context);

  return std::make_unique<chromeos_camera::CameraAppHelperImpl>(
      std::move(handle_result_callback), window);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// CameraAppUI
//
///////////////////////////////////////////////////////////////////////////////

// static
void CameraAppUI::ConnectToCameraAppDeviceProvider(
    content::RenderFrameHost* source,
    mojo::PendingReceiver<cros::mojom::CameraAppDeviceProvider> receiver) {
  auto provider =
      CreateCameraAppDeviceProvider(source->GetLastCommittedOrigin(),
                                    source->GetProcess()->GetBrowserContext());
  mojo::MakeSelfOwnedReceiver(std::move(provider), std::move(receiver));
}

// static
void CameraAppUI::ConnectToCameraAppHelper(
    content::RenderFrameHost* source,
    mojo::PendingReceiver<chromeos_camera::mojom::CameraAppHelper> receiver) {
  auto* window = source->GetNativeView()->GetToplevelWindow();
  auto helper =
      CreateCameraAppHelper(source->GetProcess()->GetBrowserContext(), window);
  mojo::MakeSelfOwnedReceiver(std::move(helper), std::move(receiver));
}

CameraAppUI::CameraAppUI(content::WebUI* web_ui,
                         std::unique_ptr<CameraAppUIDelegate> delegate)
    : ui::MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin host_origin =
      url::Origin::Create(GURL(kChromeUICameraAppURL));
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::MEDIASTREAM_MIC);
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::MEDIASTREAM_CAMERA);
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::NATIVE_FILE_SYSTEM_READ_GUARD);
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD);
  allowlist->RegisterAutoGrantedPermission(host_origin,
                                           ContentSettingsType::COOKIES);
  // The notifications permissison is needed by the IdleManager, which we use
  // for lock screen detection.
  allowlist->RegisterAutoGrantedPermission(host_origin,
                                           ContentSettingsType::NOTIFICATIONS);

  // Set up the data source.
  content::WebUIDataSource* source = CreateCameraAppUIHTMLSource();
  content::WebUIDataSource::Add(browser_context, source);
}

CameraAppUI::~CameraAppUI() = default;

void CameraAppUI::BindInterface(
    mojo::PendingReceiver<cros::mojom::CameraAppDeviceProvider> receiver) {
  provider_ = CreateCameraAppDeviceProvider(
      url::Origin::Create(GURL(chromeos::kChromeUICameraAppURL)),
      web_ui()->GetWebContents()->GetBrowserContext());
  provider_->Bind(std::move(receiver));
}

void CameraAppUI::BindInterface(
    mojo::PendingReceiver<chromeos_camera::mojom::CameraAppHelper> receiver) {
  helper_ = CreateCameraAppHelper(
      web_ui()->GetWebContents()->GetBrowserContext(), window());
  helper_->Bind(std::move(receiver));
}

aura::Window* CameraAppUI::window() {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

WEB_UI_CONTROLLER_TYPE_IMPL(CameraAppUI)

}  // namespace chromeos
