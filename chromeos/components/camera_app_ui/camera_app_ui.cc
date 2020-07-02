// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/camera_app_ui/camera_app_ui.h"

#include <utility>

#include "base/bind.h"
#include "chromeos/components/camera_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_camera_app_resources.h"
#include "chromeos/grit/chromeos_camera_app_resources_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
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

  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// CameraAppUI
//
///////////////////////////////////////////////////////////////////////////////

CameraAppUI::CameraAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
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

}  // namespace chromeos
