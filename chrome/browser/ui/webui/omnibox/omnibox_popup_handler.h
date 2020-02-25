// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_POPUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_POPUP_HANDLER_H_

#include "base/macros.h"
#include "components/omnibox/browser/omnibox_popup_model.h"

// The WebUI handler for the experimental WebUI omnibox popup.
class OmniboxPopupHandler {
 public:
  OmniboxPopupHandler();

  // These are roughly speaking analogues of the OmniboxPopupView API.
  // TODO(tommycli): Update the OmniboxPopupView API to better suit WebUI.
  void InvalidateLine(size_t line) {}
  void OnSelectionStateChanged(size_t line) {}

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_POPUP_HANDLER_H_
