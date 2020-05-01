// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/media_app_ui/media_app_ui.h"

#include <utility>

#include "chromeos/components/media_app_ui/media_app_guest_ui.h"
#include "chromeos/components/media_app_ui/media_app_page_handler.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/components/web_applications/manifest_request_filter.h"
#include "chromeos/grit/chromeos_media_app_bundle_resources.h"
#include "chromeos/grit/chromeos_media_app_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace chromeos {
namespace {

content::WebUIDataSource* CreateHostDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIMediaAppHost);

  // Add resources from chromeos_media_app_resources.pak.
  source->SetDefaultResource(IDR_MEDIA_APP_INDEX_HTML);
  source->AddResourcePath("pwa.html", IDR_MEDIA_APP_PWA_HTML);
  source->AddResourcePath("mojo_api_bootstrap.js",
                          IDR_MEDIA_APP_MOJO_API_BOOTSTRAP_JS);
  source->AddResourcePath("media_app.mojom-lite.js",
                          IDR_MEDIA_APP_MEDIA_APP_MOJOM_JS);
  source->AddResourcePath("media_app_index_scripts.js",
                          IDR_MEDIA_APP_INDEX_SCRIPTS_JS);
  source->AddLocalizedString("appTitle", IDS_MEDIA_APP_APP_NAME);
  web_app::SetManifestRequestFilter(source, IDR_MEDIA_APP_MANIFEST,
                                    IDS_MEDIA_APP_APP_NAME);

  // TODO(b/141588875): Switch this back to IDR_MEDIA_APP_APP_ICON_256_PNG (and
  // add more icon resolutions) when the final icon is ready.
  source->AddResourcePath("system_assets/app_icon_256.png",
                          IDR_MEDIA_APP_GALLERY_ICON_256_PNG);

  return source;
}

}  // namespace

MediaAppUI::MediaAppUI(content::WebUI* web_ui,
                       std::unique_ptr<MediaAppUIDelegate> delegate)
    : MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* host_source = CreateHostDataSource();
  content::WebUIDataSource::Add(browser_context, host_source);

  // The guest is in an <iframe>. Add it to CSP.
  std::string csp = std::string("frame-src ") + kChromeUIMediaAppGuestURL + ";";
  host_source->OverrideContentSecurityPolicyChildSrc(csp);

  content::WebUIDataSource* untrusted_source =
      CreateMediaAppUntrustedDataSource();
  content::WebUIDataSource::Add(browser_context, untrusted_source);

  // Add ability to request chrome-untrusted: URLs.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

MediaAppUI::~MediaAppUI() = default;

void MediaAppUI::BindInterface(
    mojo::PendingReceiver<media_app_ui::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void MediaAppUI::CreatePageHandler(
    mojo::PendingReceiver<media_app_ui::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<MediaAppPageHandler>(this, std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MediaAppUI)

}  // namespace chromeos
