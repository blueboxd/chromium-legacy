// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/internals_ui.h"

#include "chrome/browser/ui/webui/internals/query_tiles/query_tiles_internals_ui_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

InternalsUI::InternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  profile_ = Profile::FromWebUI(web_ui);
  source_ = content::WebUIDataSource::Create(chrome::kChromeUIInternalsHost);

  // Add your sub-URL internals WebUI here.
  AddQueryTilesInternals(web_ui);

  content::WebUIDataSource::Add(profile_, source_);
}

InternalsUI::~InternalsUI() = default;

void InternalsUI::AddQueryTilesInternals(content::WebUI* web_ui) {
  source_->AddResourcePath("query_tiles_internals.js",
                           IDR_QUERY_TILES_INTERNALS_JS);
  source_->AddResourcePath("query_tiles_internals_browser_proxy.js",
                           IDR_QUERY_TILES_INTERNALS_BROWSER_PROXY_JS);
  source_->AddResourcePath("query-tiles", IDR_QUERY_TILES_INTERNALS_HTML);
  web_ui->AddMessageHandler(
      std::make_unique<QueryTilesInternalsUIMessageHandler>(profile_));
}
