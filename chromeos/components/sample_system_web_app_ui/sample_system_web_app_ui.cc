// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sample_system_web_app_ui/sample_system_web_app_ui.h"

#include <memory>

#include "chromeos/components/sample_system_web_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_sample_system_web_app_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

SampleSystemWebAppUI::SampleSystemWebAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  auto html_source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUISampleSystemWebAppHost));

  html_source->AddResourcePath("", IDR_SAMPLE_SYSTEM_WEB_APP_INDEX_HTML);
  html_source->AddResourcePath("pwa.html", IDR_SAMPLE_SYSTEM_WEB_APP_PWA_HTML);
  html_source->AddResourcePath("app.js", IDR_SAMPLE_SYSTEM_WEB_APP_JS);
  html_source->AddResourcePath("manifest.json",
                               IDR_SAMPLE_SYSTEM_WEB_APP_MANIFEST);
  html_source->AddResourcePath("app_icon_192.png",
                               IDR_SAMPLE_SYSTEM_WEB_APP_ICON_192);

#if !DCHECK_IS_ON()
  // If a user goes to an invalid url and non-DCHECK mode (DHECK = debug mode)
  // is set, serve a default page so the user sees your default page instead
  // of an unexpected error. But if DCHECK is set, the user will be a
  // developer and be able to identify an error occurred.
  html_source->SetDefaultResource(IDR_SAMPLE_SYSTEM_WEB_APP_INDEX_HTML);
#endif  // !DCHECK_IS_ON()

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source.release());
}

SampleSystemWebAppUI::~SampleSystemWebAppUI() = default;

}  // namespace chromeos
