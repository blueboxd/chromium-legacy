// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/profile_picker_resources.h"
#include "chrome/grit/profile_picker_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

ProfilePickerUI::ProfilePickerUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIProfilePickerHost);

  std::string generated_path =
      "@out_folder@/gen/chrome/browser/resources/signin/profile_picker/";
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kProfilePickerResources, kProfilePickerResourcesSize),
      generated_path, IDR_PROFILE_PICKER_PROFILE_PICKER_HTML);

  content::WebUIDataSource::Add(profile, html_source);
}

ProfilePickerUI::~ProfilePickerUI() = default;
