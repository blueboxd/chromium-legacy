// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"

#include <string>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/country_codes/country_codes.h"
#include "content/public/browser/web_ui.h"
#include "net/dns/public/doh_provider_list.h"
#include "ui/base/l10n/l10n_util.h"

using chrome_browser_net::SecureDnsUiManagementMode;

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
  SecureDnsUiManagementMode management_mode;
  SystemNetworkContextManager::GetStubResolverConfig(
      g_browser_process->local_state(), &insecure_stub_resolver_enabled,
      &secure_dns_mode, &dns_over_https_servers, &management_mode);

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

  auto secure_dns_templates = std::make_unique<base::ListValue>();
  if (dns_over_https_servers.has_value()) {
    for (const auto& doh_server : *dns_over_https_servers) {
      secure_dns_templates->Append(doh_server->server_template);
    }
  }

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString("mode", secure_dns_mode_str);
  dict->SetList("templates", std::move(secure_dns_templates));
  dict->SetInteger("managementMode", static_cast<int>(management_mode));
  return dict;
}

}  // namespace

SecureDnsHandler::SecureDnsHandler() = default;

SecureDnsHandler::~SecureDnsHandler() = default;

void SecureDnsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSecureDnsResolverList",
      base::BindRepeating(&SecureDnsHandler::HandleGetSecureDnsResolverList,
                          base::Unretained(this)));

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
  pref_registrar_.Add(
      prefs::kDnsOverHttpsTemplates,
      base::Bind(&SecureDnsHandler::SendSecureDnsSettingUpdatesToJavascript,
                 base::Unretained(this)));
}

void SecureDnsHandler::OnJavascriptDisallowed() {
  pref_registrar_.RemoveAll();
}

base::Value SecureDnsHandler::GetSecureDnsResolverListForCountry(
    int country_id,
    const std::vector<net::DohProviderEntry>& providers) {
  std::vector<std::string> disabled_providers =
      SplitString(features::kDnsOverHttpsDisabledProvidersParam.Get(), ",",
                  base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  base::Value resolvers(base::Value::Type::LIST);
  // Add all non-disabled resolvers that should be displayed in |country_id|.
  for (const auto& entry : providers) {
    if (base::Contains(disabled_providers, entry.provider))
      continue;

    if (entry.display_globally ||
        std::find_if(
            entry.display_countries.begin(), entry.display_countries.end(),
            [&country_id](const std::string& country_code) {
              return country_codes::CountryCharsToCountryID(
                         country_code[0], country_code[1]) == country_id;
            }) != entry.display_countries.end()) {
      DCHECK(!entry.ui_name.empty());
      DCHECK(!entry.privacy_policy.empty());
      base::Value dict(base::Value::Type::DICTIONARY);
      dict.SetKey("name", base::Value(entry.ui_name));
      dict.SetKey("value", base::Value(entry.dns_over_https_template));
      dict.SetKey("policy", base::Value(entry.privacy_policy));
      resolvers.Append(std::move(dict));
    }
  }

  // Randomize the order of the resolvers.
  base::RandomShuffle(resolvers.GetList().begin(), resolvers.GetList().end());

  // Add a custom option to the front of the list
  base::Value custom(base::Value::Type::DICTIONARY);
  custom.SetKey("name",
                base::Value(l10n_util::GetStringUTF8(IDS_SETTINGS_CUSTOM)));
  custom.SetKey("value", base::Value("custom"));
  custom.SetKey("policy", base::Value(std::string()));
  resolvers.Insert(resolvers.GetList().begin(), std::move(custom));

  return resolvers;
}

void SecureDnsHandler::HandleGetSecureDnsResolverList(
    const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id = args->GetList()[0].GetString();

  ResolveJavascriptCallback(
      base::Value(callback_id),
      GetSecureDnsResolverListForCountry(country_codes::GetCurrentCountryID(),
                                         net::GetDohProviderList()));
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
