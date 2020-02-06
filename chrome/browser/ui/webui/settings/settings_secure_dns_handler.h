// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURE_DNS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURE_DNS_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"

namespace settings {

// Handler for the Secure DNS setting.
class SecureDnsHandler : public SettingsPageUIHandler {
 public:
  SecureDnsHandler();
  ~SecureDnsHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 protected:
  // Intended to be called once upon creation of the secure DNS setting.
  void HandleGetSecureDnsSetting(const base::ListValue* args);

  // Retrieves the current host resolver configuration, computes the
  // corresponding UI representation, and sends it to javascript.
  void SendSecureDnsSettingUpdatesToJavascript();

 private:
  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SecureDnsHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURE_DNS_HANDLER_H_
