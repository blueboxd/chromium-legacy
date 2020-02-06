// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"

#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_ui.h"

namespace settings {

namespace {

std::unique_ptr<base::DictionaryValue> CreateSecureDnsSettingDict() {
  // Fetch the current host resolver configuration. It is not sufficient to read
  // the secure DNS prefs directly since the host resolver configuration takes
  // other factors into account such as whether a managed environment or
  // parental controls have been detected.
  bool insecure_stub_resolver_enabled = false;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
      dns_over_https_servers;
  SystemNetworkContextManager::GetStubResolverConfig(
      g_browser_process->local_state(), &insecure_stub_resolver_enabled,
      &secure_dns_mode, &dns_over_https_servers);

  std::string secure_dns_mode_str;
  switch (secure_dns_mode) {
    case net::DnsConfig::SecureDnsMode::SECURE:
      secure_dns_mode_str = chrome_browser_net::kDnsOverHttpsModeSecure;
      break;
    case net::DnsConfig::SecureDnsMode::AUTOMATIC:
      secure_dns_mode_str = chrome_browser_net::kDnsOverHttpsModeAutomatic;
      break;
    case net::DnsConfig::SecureDnsMode::OFF:
      secure_dns_mode_str = chrome_browser_net::kDnsOverHttpsModeOff;
      break;
    default:
      NOTREACHED();
  }

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("mode", secure_dns_mode_str);
  return dict;
}

}  // namespace

SecureDnsHandler::SecureDnsHandler() = default;

SecureDnsHandler::~SecureDnsHandler() = default;

void SecureDnsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSecureDnsSetting",
      base::BindRepeating(&SecureDnsHandler::HandleGetSecureDnsSetting,
                          base::Unretained(this)));
}

void SecureDnsHandler::OnJavascriptAllowed() {
  // Register for updates to the underlying secure DNS prefs so that the
  // secure DNS setting can be updated to reflect the current host resolver
  // configuration.
  pref_registrar_.Init(g_browser_process->local_state());
  pref_registrar_.Add(
      prefs::kDnsOverHttpsMode,
      base::Bind(&SecureDnsHandler::SendSecureDnsSettingUpdatesToJavascript,
                 base::Unretained(this)));
}

void SecureDnsHandler::OnJavascriptDisallowed() {
  pref_registrar_.RemoveAll();
}

void SecureDnsHandler::HandleGetSecureDnsSetting(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1u, args->GetList().size());
  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, *CreateSecureDnsSettingDict());
}

void SecureDnsHandler::SendSecureDnsSettingUpdatesToJavascript() {
  FireWebUIListener("secure-dns-setting-changed",
                    *CreateSecureDnsSettingDict());
}

}  // namespace settings
