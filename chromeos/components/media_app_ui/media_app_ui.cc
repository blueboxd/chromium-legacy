// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/media_app_ui/media_app_ui.h"

#include "chromeos/components/media_app_ui/media_app_guest_ui.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace {

content::WebUIDataSource* CreateHostDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIMediaAppHost);
  source->SetDefaultResource(IDR_MEDIA_APP_INDEX_HTML);
  return source;
}

}  // namespace

MediaAppUI::MediaAppUI(content::WebUI* web_ui) : MojoWebUIController(web_ui) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* host_source = CreateHostDataSource();
  content::WebUIDataSource::Add(browser_context, host_source);

  // Whilst the guest is in an <iframe> rather than a <webview>, we need a CSP
  // override to use the guest origin in the host.
  // TODO(crbug/996088): Remove these overrides when there's a new sandboxing
  // option for the guest.
  std::string csp = std::string("frame-src ") + kChromeUIMediaAppGuestURL + ";";
  host_source->OverrideContentSecurityPolicyChildSrc(csp);

  // We also add the guest data source here (and allow it to be iframed). If
  // it's only added in the MediaAppGuestUI constructor, then a user navigation
  // to chrome://media-app-guest is required before the <iframe> can see it.
  // This is due to logic in RenderFrameHostManager::GetFrameHostForNavigation()
  // that currently skips creating webui objects when !IsMainFrame() (which is
  // temporary according to https://crbug.com/713313 but, long-term, the guest
  // shouldn't need the webui objects in any case - just the data source).
  content::WebUIDataSource* guest_source = MediaAppGuestUI::CreateDataSource();
  guest_source->DisableDenyXFrameOptions();
  content::WebUIDataSource::Add(browser_context, guest_source);
}

MediaAppUI::~MediaAppUI() = default;

}  // namespace chromeos
