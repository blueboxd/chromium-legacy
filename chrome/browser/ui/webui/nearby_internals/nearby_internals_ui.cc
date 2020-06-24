// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui.h"

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/nearby_internals_resources.h"
#include "chrome/grit/nearby_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace {
constexpr char kNearbyInternalsGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/nearby_internals/";
}  // namespace

NearbyInternalsUI::NearbyInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  // Nearby Sharing is not available to incognito or guest profiles.
  DCHECK(profile->IsRegularProfile());
  DCHECK(base::FeatureList::IsEnabled(features::kNearbySharing));

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUINearbyInternalsHost);

  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kNearbyInternalsResources, kNearbyInternalsResourcesSize),
      kNearbyInternalsGeneratedPath, IDR_NEARBY_INTERNALS_INDEX_HTML);

  content::WebUIDataSource::Add(profile, html_source);
}

NearbyInternalsUI::~NearbyInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(NearbyInternalsUI)
