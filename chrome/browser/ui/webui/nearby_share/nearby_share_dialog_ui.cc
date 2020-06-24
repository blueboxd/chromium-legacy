// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/nearby_share_dialog_resources.h"
#include "chrome/grit/nearby_share_dialog_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

constexpr char kNearbyShareGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/nearby_share/";

}  // namespace

namespace nearby_share {

NearbyShareDialogUI::NearbyShareDialogUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  // Nearby Share is not available to incognito or guest profiles.
  DCHECK(profile->IsRegularProfile());

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUINearbyShareHost);

  webui::SetupWebUIDataSource(html_source,
                              base::make_span(kNearbyShareDialogResources,
                                              kNearbyShareDialogResourcesSize),
                              kNearbyShareGeneratedPath,
                              IDR_NEARBY_SHARE_NEARBY_SHARE_DIALOG_HTML);

  content::WebUIDataSource::Add(profile, html_source);
}

NearbyShareDialogUI::~NearbyShareDialogUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(NearbyShareDialogUI)

}  // namespace nearby_share
