// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_COOKIE_CONTROLS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_COOKIE_CONTROLS_HANDLER_H_

#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

class CookieControlsHandlerTest;

namespace base {
class ListValue;
}

// Handles requests for prefs::kCookieControlsMode retrival/update.
class CookieControlsHandler : public content::WebUIMessageHandler {
 public:
  CookieControlsHandler();
  ~CookieControlsHandler() override;

  // WebUIMessageHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void HandleCookieControlsToggleChanged(const base::ListValue* args);

  void HandleObserveCookieControlsModeChange(const base::ListValue* args);

 private:
  friend class CookieControlsHandlerTest;

  void OnCookieControlsChanged();

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(CookieControlsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_COOKIE_CONTROLS_HANDLER_H_
