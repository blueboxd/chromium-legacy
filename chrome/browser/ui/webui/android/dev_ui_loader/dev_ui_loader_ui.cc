// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/android/dev_ui_loader/dev_ui_loader_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/android/dev_ui_loader/dev_ui_loader_message_handler.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "url/gurl.h"

DevUiLoaderUI::DevUiLoaderUI(content::WebUI* web_ui_in, const GURL& url)
    : WebUIController(web_ui_in) {
  std::unique_ptr<content::WebUIDataSource> html_source;
  html_source.reset(content::WebUIDataSource::Create(url.host()));
  html_source->SetDefaultResource(IDR_DEV_UI_LOADER_HTML);
  html_source->AddResourcePath("dev_ui_loader.html", IDR_DEV_UI_LOADER_HTML);
  html_source->AddResourcePath("dev_ui_loader.js", IDR_DEV_UI_LOADER_JS);
  html_source->AddResourcePath("dev_ui_loader.css", IDR_DEV_UI_LOADER_CSS);

  Profile* profile = Profile::FromWebUI(web_ui());
  content::WebUIDataSource::Add(profile, html_source.release());
  web_ui()->AddMessageHandler(std::make_unique<DevUiLoaderMessageHandler>());
}

DevUiLoaderUI::~DevUiLoaderUI() = default;
