// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/help_app_ui.h"

#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_help_app_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

namespace {
content::WebUIDataSource* CreateDataSource() {
  auto* source = content::WebUIDataSource::Create(kChromeUIHelpAppHost);

  // TODO(crbug.com/1012578): This is a placeholder only, update with the
  // actual app content.
  source->SetDefaultResource(IDR_HELP_APP_INDEX_HTML);
  return source;
}
}  // namespace

HelpAppUI::HelpAppUI(content::WebUI* web_ui) : MojoWebUIController(web_ui) {
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                CreateDataSource());
}

HelpAppUI::~HelpAppUI() = default;

}  // namespace chromeos
