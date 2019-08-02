// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/tab_strip_resources.h"
#include "chrome/grit/tab_strip_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

TabStripUI::TabStripUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUITabStripHost);

  std::string generated_path =
      "@out_folder@/gen/chrome/browser/resources/tab_strip/";

  for (size_t i = 0; i < kTabStripResourcesSize; ++i) {
    std::string path = kTabStripResources[i].name;
    if (path.rfind(generated_path, 0) == 0) {
      path = path.substr(generated_path.length());
    }
    html_source->AddResourcePath(path, kTabStripResources[i].value);
  }

  html_source->SetDefaultResource(IDR_TAB_STRIP_HTML);

  content::WebUIDataSource::Add(profile, html_source);
}

TabStripUI::~TabStripUI() {}
