// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_url_disabled_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"

ChromeURLDisabledUI::ChromeURLDisabledUI(content::WebUI* web_ui,
                                         const std::string& host_name)
    : content::WebUIController(web_ui), weak_factory_(this) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(host_name);

  html_source->UseStringsJs();

  html_source->AddLocalizedString("disabledPageHeader",
                                  IDS_CHROME_URLS_DISABLED_PAGE_HEADER);
  html_source->AddLocalizedString("disabledPageTitle",
                                  IDS_CHROME_URLS_DISABLED_PAGE_TITLE);
  html_source->AddLocalizedString("disabledPageMessage",
                                  IDS_CHROME_URLS_DISABLED_PAGE_MESSAGE);
  html_source->SetDefaultResource(IDR_CHROME_URLS_DISABLED_PAGE_HTML);

  html_source->SetDefaultResource(IDR_CHROME_URLS_DISABLED_PAGE_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

ChromeURLDisabledUI::~ChromeURLDisabledUI() = default;
