// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/webui_omnibox_popup_view.h"

#include "chrome/common/webui_url_constants.h"

WebUIOmniboxPopupView::WebUIOmniboxPopupView(
    content::BrowserContext* browser_context)
    : views::WebView(browser_context) {
  // TODO(tommycli): Replace with actual sizing logic.
  SetPreferredSize(gfx::Size(600, 300));

  // TODO(tommycli): Replace with the actual omnibox popup WebUI surface.
  // It's not implemented yet.
  constexpr char kOmniboxPopupUrl[] = "chrome://omnibox/omnibox_popup.html";
  LoadInitialURL(GURL(kOmniboxPopupUrl));
}
