// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURE_DNS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURE_DNS_HANDLER_H_

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/dns/public/doh_provider_list.h"

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

  // Get the list of dropdown resolver options. Each option is represented
  // as a dictionary with the following keys: "name" (the text to display in the
  // UI), "value" (the DoH template for this provider), and "policy" (the URL of
  // the provider's privacy policy).
  base::Value GetSecureDnsResolverListForCountry(
      int country_id,
      const std::vector<net::DohProviderEntry>& providers);

 protected:
  // Retrieves all pre-approved secure resolvers and returns them to WebUI.
  void HandleGetSecureDnsResolverList(const base::ListValue* args);

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
