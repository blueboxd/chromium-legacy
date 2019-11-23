// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/known_interception_disclosure_ui.h"

#include "build/build_config.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/content/urls.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

KnownInterceptionDisclosureUI::KnownInterceptionDisclosureUI(
    content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      kChromeUIConnectionMonitoringDetectedHost);

  html_source->AddLocalizedString("title", IDS_KNOWN_INTERCEPTION_TITLE);
  html_source->AddLocalizedString("pageSubtitle",
                                  IDS_KNOWN_INTERCEPTION_SUBTITLE);
  html_source->AddLocalizedString("pageDescription",
                                  IDS_KNOWN_INTERCEPTION_DESCRIPTION);
  html_source->AddLocalizedString("pageMeaningSubheading",
                                  IDS_KNOWN_INTERCEPTION_MEANING_SUBHEADING);
  html_source->AddLocalizedString("pageMeaningDescription",
                                  IDS_KNOWN_INTERCEPTION_MEANING_DESCRIPTION);
  html_source->AddLocalizedString("pageCauseSubheading",
                                  IDS_KNOWN_INTERCEPTION_CAUSE_SUBHEADING);
  html_source->AddLocalizedString("pageCauseDescription",
                                  IDS_KNOWN_INTERCEPTION_CAUSE_DESCRIPTION);

  html_source->AddResourcePath("interstitial_core.css",
                               IDR_SECURITY_INTERSTITIAL_CORE_CSS);
  html_source->AddResourcePath("interstitial_common.css",
                               IDR_SECURITY_INTERSTITIAL_COMMON_CSS);
  html_source->AddResourcePath("monitoring_disclosure.css",
                               IDR_KNOWN_INTERCEPTION_CSS);
  html_source->AddResourcePath("images/1x/triangle_red.png",
                               IDR_KNOWN_INTERCEPTION_ICON_1X_PNG);
  html_source->AddResourcePath("images/2x/triangle_red.png",
                               IDR_KNOWN_INTERCEPTION_ICON_2X_PNG);
  html_source->SetDefaultResource(IDR_KNOWN_INTERCEPTION_HTML);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, html_source);
}

KnownInterceptionDisclosureUI::~KnownInterceptionDisclosureUI() = default;

}  // namespace security_interstitials
