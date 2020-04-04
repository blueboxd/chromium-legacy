// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/country_codes/country_codes.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/doh_provider_list.h"
#include "net/dns/public/util.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

namespace {

const char kProbeHostname[] = "google.com";

std::unique_ptr<base::DictionaryValue> CreateSecureDnsSettingDict() {
  // Fetch the current host resolver configuration. It is not sufficient to read
  // the secure DNS prefs directly since the host resolver configuration takes
  // other factors into account such as whether a managed environment or
  // parental controls have been detected.
  bool insecure_stub_resolver_enabled = false;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers;
  chrome_browser_net::SecureDnsUiManagementMode management_mode;
  SystemNetworkContextManager::GetStubResolverConfigReader()->GetConfiguration(
      true /* force_check_parental_controls_for_automatic_mode */,
      &insecure_stub_resolver_enabled, &secure_dns_mode,
      &dns_over_https_servers, &management_mode);

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
  for (const auto& doh_server : dns_over_https_servers) {
    secure_dns_templates->Append(doh_server.server_template);
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

  web_ui()->RegisterMessageCallback(
      "validateCustomDnsEntry",
      base::BindRepeating(&SecureDnsHandler::HandleValidateCustomDnsEntry,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "probeCustomDnsTemplate",
      base::BindRepeating(&SecureDnsHandler::HandleProbeCustomDnsTemplate,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "recordUserDropdownInteraction",
      base::BindRepeating(
          &SecureDnsHandler::HandleRecordUserDropdownInteraction,
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
  resolver_histogram_map_.clear();
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
      DCHECK(entry.provider_id_for_histogram.has_value());
      resolver_histogram_map_.insert({entry.dns_over_https_template,
                                      entry.provider_id_for_histogram.value()});
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
  resolver_histogram_map_.insert(
      {"custom", net::DohProviderIdForHistogram::kCustom});

  return resolvers;
}

void SecureDnsHandler::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  network_context_for_testing_ = network_context;
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

void SecureDnsHandler::HandleValidateCustomDnsEntry(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  std::string custom_entry;
  CHECK(args->Get(0, &callback_id));
  CHECK(args->GetString(1, &custom_entry));

  // Return the first template, or none if the entry is invalid.
  std::string first_template;
  bool valid = !custom_entry.empty() &&
               chrome_browser_net::IsValidDohTemplateGroup(custom_entry);
  if (valid) {
    first_template =
        std::string(chrome_browser_net::SplitDohTemplateGroup(custom_entry)[0]);
  }
  UMA_HISTOGRAM_BOOLEAN("Net.DNS.UI.ValidationAttemptSuccess", valid);
  ResolveJavascriptCallback(*callback_id, base::Value(first_template));
}

void SecureDnsHandler::HandleProbeCustomDnsTemplate(
    const base::ListValue* args) {
  AllowJavascript();
  receiver_.reset();
  host_resolver_.reset();

  std::string server_template;
  CHECK(args->GetString(0, &probe_callback_id_));
  CHECK(args->GetString(1, &server_template));

  net::DnsConfigOverrides overrides;
  overrides.search = std::vector<std::string>();
  overrides.attempts = 1;
  overrides.randomize_ports = false;
  overrides.secure_dns_mode = net::DnsConfig::SecureDnsMode::SECURE;
  std::string server_method;
  // We only send probe queries to templates that have already passed a format
  // validation check.
  CHECK(net::dns_util::IsValidDohTemplate(server_template, &server_method));
  overrides.dns_over_https_servers.emplace({net::DnsOverHttpsServerConfig(
      server_template, server_method == "POST")});
  auto* network_context =
      network_context_for_testing_
          ? network_context_for_testing_
          : content::BrowserContext::GetDefaultStoragePartition(
                web_ui()->GetWebContents()->GetBrowserContext())
                ->GetNetworkContext();
  network_context->CreateHostResolver(
      overrides, host_resolver_.BindNewPipeAndPassReceiver());

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::A;
  parameters->source = net::HostResolverSource::DNS;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;
  host_resolver_->ResolveHost(net::HostPortPair(kProbeHostname, 80),
                              net::NetworkIsolationKey::CreateTransient(),
                              std::move(parameters),
                              receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &SecureDnsHandler::OnMojoConnectionError, base::Unretained(this)));
}

void SecureDnsHandler::HandleRecordUserDropdownInteraction(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string old_provider;
  std::string new_provider;
  CHECK(args->GetString(0, &old_provider));
  CHECK(args->GetString(1, &new_provider));
  DCHECK(resolver_histogram_map_.find(old_provider) !=
         resolver_histogram_map_.end());
  DCHECK(resolver_histogram_map_.find(new_provider) !=
         resolver_histogram_map_.end());
  for (auto& pair : resolver_histogram_map_) {
    if (pair.first == old_provider) {
      UMA_HISTOGRAM_ENUMERATION("Net.DNS.UI.DropdownSelectionEvent.Unselected",
                                pair.second);
    } else if (pair.first == new_provider) {
      UMA_HISTOGRAM_ENUMERATION("Net.DNS.UI.DropdownSelectionEvent.Selected",
                                pair.second);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Net.DNS.UI.DropdownSelectionEvent.Ignored",
                                pair.second);
    }
  }
}

// network::ResolveHostClientBase impl:
void SecureDnsHandler::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const base::Optional<net::AddressList>& resolved_addresses) {
  receiver_.reset();
  host_resolver_.reset();
  UMA_HISTOGRAM_BOOLEAN("Net.DNS.UI.ProbeAttemptSuccess", (result == 0));
  ResolveJavascriptCallback(base::Value(probe_callback_id_),
                            base::Value(result == 0));
}

void SecureDnsHandler::OnMojoConnectionError() {
  OnComplete(net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
             base::nullopt);
}

void SecureDnsHandler::SendSecureDnsSettingUpdatesToJavascript() {
  FireWebUIListener("secure-dns-setting-changed",
                    *CreateSecureDnsSettingDict());
}

}  // namespace settings
