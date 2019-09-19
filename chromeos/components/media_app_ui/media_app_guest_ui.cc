// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/media_app_ui/media_app_guest_ui.h"

#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

// static
content::WebUIDataSource* MediaAppGuestUI::CreateDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIMediaAppGuestHost);
  source->AddResourcePath("app.html", IDR_MEDIA_APP_APP_HTML);
  source->AddResourcePath("js/app_main.js", IDR_MEDIA_APP_APP_JS);
  source->AddResourcePath("js/app_image_handler_module.js",
                          IDR_MEDIA_APP_IMAGE_HANDLER_MODULE_JS);
  source->AddResourcePath("js/app_drop_target_module.js",
                          IDR_MEDIA_APP_DROP_TARGET_MODULE_JS);
  return source;
}

MediaAppGuestUI::MediaAppGuestUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui) {
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                CreateDataSource());
}

MediaAppGuestUI::~MediaAppGuestUI() = default;

}  // namespace chromeos
