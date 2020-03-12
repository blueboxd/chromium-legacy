// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/print_management/scanning_ui.h"

#include "base/memory/ptr_util.h"
#include "chromeos/components/print_management/url_constants.h"
#include "chromeos/grit/chromeos_print_management_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/resources/grit/webui_resources.h"

namespace chromeos {

ScanningUI::ScanningUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  auto html_source =
      base::WrapUnique(content::WebUIDataSource::Create(kChromeUIScanningHost));
  html_source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources chrome://test 'self';");

  html_source->AddResourcePath("scanning_ui.js", IDR_SCANNING_UI_JS);
  html_source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER);
  html_source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER);
  html_source->SetDefaultResource(IDR_SCANNING_UI_INDEX_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source.release());
}

ScanningUI::~ScanningUI() = default;

}  // namespace chromeos
