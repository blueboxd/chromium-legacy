// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/help_app_ui.h"

#include <utility>

#include "chromeos/components/help_app_ui/help_app_page_handler.h"
#include "chromeos/components/help_app_ui/help_app_untrusted_ui.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/components/web_applications/manifest_request_filter.h"
#include "chromeos/grit/chromeos_help_app_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace chromeos {

namespace {
content::WebUIDataSource* CreateHostDataSource() {
  auto* source = content::WebUIDataSource::Create(kChromeUIHelpAppHost);

  // TODO(crbug.com/1012578): This is a placeholder only, update with the
  // actual app content.
  source->SetDefaultResource(IDR_HELP_APP_HOST_INDEX_HTML);
  source->AddResourcePath("pwa.html", IDR_HELP_APP_PWA_HTML);
  source->AddResourcePath("app_icon_192.png", IDR_HELP_APP_ICON_192);
  source->AddResourcePath("app_icon_512.png", IDR_HELP_APP_ICON_512);
  source->AddResourcePath("browser_proxy.js", IDR_HELP_APP_BROWSER_PROXY_JS);
  source->AddResourcePath("help_app.mojom-lite.js",
                          IDR_HELP_APP_HELP_APP_MOJOM_JS);
  source->AddLocalizedString("appTitle", IDS_HELP_APP_DISCOVER);
  web_app::SetManifestRequestFilter(source, IDR_HELP_APP_MANIFEST,
                                    IDS_HELP_APP_DISCOVER);
  return source;
}
}  // namespace

HelpAppUI::HelpAppUI(content::WebUI* web_ui,
                     std::unique_ptr<HelpAppUIDelegate> delegate)
    : MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* host_source = CreateHostDataSource();
  content::WebUIDataSource::Add(browser_context, host_source);
  // We need a CSP override to use the chrome-untrusted:// scheme in the host.
  std::string csp =
      std::string("frame-src ") + kChromeUIHelpAppUntrustedURL + ";";
  host_source->OverrideContentSecurityPolicyChildSrc(csp);

  content::WebUIDataSource* untrusted_source =
      CreateHelpAppUntrustedDataSource(delegate_.get());

  content::WebUIDataSource::Add(browser_context, untrusted_source);

  // Add ability to request chrome-untrusted: URLs.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

HelpAppUI::~HelpAppUI() = default;

void HelpAppUI::BindInterface(
    mojo::PendingReceiver<help_app_ui::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void HelpAppUI::CreatePageHandler(
    mojo::PendingReceiver<help_app_ui::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<HelpAppPageHandler>(this, std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(HelpAppUI)

}  // namespace chromeos
