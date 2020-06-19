// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNALS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNALS_INTERNALS_UI_H_

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace content {
class WebUI;
}  // namespace content

// Client could put debug WebUI as sub-URL under chrome://internals/.
// e.g. chrome://internals/your-feature.
class InternalsUI : public content::WebUIController {
 public:
  explicit InternalsUI(content::WebUI* web_ui);
  ~InternalsUI() override;

 private:
  // Add resources and message handler for chrome://internals/query-tiles.
  void AddQueryTilesInternals(content::WebUI* web_ui);

  Profile* profile_;
  content::WebUIDataSource* source_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNALS_INTERNALS_UI_H_
