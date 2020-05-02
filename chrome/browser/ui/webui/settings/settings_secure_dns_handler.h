// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURE_DNS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURE_DNS_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/net/dns_probe_runner.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/dns/public/doh_provider_list.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/network_context.mojom.h"

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

  void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context);

 protected:
  // Retrieves all pre-approved secure resolvers and returns them to WebUI.
  void HandleGetSecureDnsResolverList(const base::ListValue* args);

  // Intended to be called once upon creation of the secure DNS setting.
  void HandleGetSecureDnsSetting(const base::ListValue* args);

  // Parses a custom entry into templates, if they are all valid.
  void HandleParseCustomDnsEntry(const base::ListValue* args);

  // Returns whether or not a test query to the resolver succeeds.
  void HandleProbeCustomDnsTemplate(const base::ListValue* args);

  // Records metrics on the user-initiated dropdown selection event.
  void HandleRecordUserDropdownInteraction(const base::ListValue* args);

  // Retrieves the current host resolver configuration, computes the
  // corresponding UI representation, and sends it to javascript.
  void SendSecureDnsSettingUpdatesToJavascript();

 private:
  network::mojom::NetworkContext* GetNetworkContext();
  void OnProbeComplete();

  std::map<std::string, net::DohProviderIdForHistogram> resolver_histogram_map_;
  std::unique_ptr<chrome_browser_net::DnsProbeRunner> runner_;
  chrome_browser_net::DnsProbeRunner::NetworkContextGetter
      network_context_getter_;
  // ID of the Javascript callback for the current pending probe, or "" if
  // there is no probe currently in progress.
  std::string probe_callback_id_;
  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SecureDnsHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURE_DNS_HANDLER_H_
