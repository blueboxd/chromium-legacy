// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/stub_resolver_config_reader.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/dns/public/util.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

#if defined(OS_WIN)
#include "base/enterprise_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/win/parental_controls.h"
#endif

namespace {

// Detailed descriptions of the secure DNS mode. These values are logged to UMA.
// Entries should not be renumbered and numeric values should never be reused.
// Please keep in sync with "SecureDnsModeDetails" in
// src/tools/metrics/histograms/enums.xml.
enum class SecureDnsModeDetailsForHistogram {
  // The mode is controlled by the user and is set to 'off'.
  kOffByUser = 0,
  // The mode is controlled via enterprise policy and is set to 'off'.
  kOffByEnterprisePolicy = 1,
  // Chrome detected a managed environment and forced the mode to 'off'.
  kOffByDetectedManagedEnvironment = 2,
  // Chrome detected parental controls and forced the mode to 'off'.
  kOffByDetectedParentalControls = 3,
  // The mode is controlled by the user and is set to 'automatic' (the default
  // mode).
  kAutomaticByUser = 4,
  // The mode is controlled via enterprise policy and is set to 'automatic'.
  kAutomaticByEnterprisePolicy = 5,
  // The mode is controlled by the user and is set to 'secure'.
  kSecureByUser = 6,
  // The mode is controlled via enterprise policy and is set to 'secure'.
  kSecureByEnterprisePolicy = 7,
  kMaxValue = kSecureByEnterprisePolicy,
};

#if defined(OS_WIN)
bool ShouldDisableDohForWindowsParentalControls() {
  const WinParentalControls& parental_controls = GetWinParentalControls();
  if (parental_controls.web_filter)
    return true;

  // Some versions before Windows 8 may not fully support |web_filter|, so
  // conservatively disable doh for any recognized parental controls.
  if (parental_controls.any_restrictions &&
    base::win::GetVersion() < base::win::Version::WIN8) {
    return true;
  }

  return false;
}
#endif  // defined(OS_WIN)

}  // namespace

StubResolverConfigReader::StubResolverConfigReader(PrefService* local_state) :
  local_state_(local_state) {}

void StubResolverConfigReader::GetConfiguration(
    bool* insecure_stub_resolver_enabled,
    net::DnsConfig::SecureDnsMode* secure_dns_mode,
    base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>*
        dns_over_https_servers,
    bool record_metrics,
    chrome_browser_net::SecureDnsUiManagementMode* forced_management_mode) {
  DCHECK(!dns_over_https_servers->has_value());

  *insecure_stub_resolver_enabled =
      local_state_->GetBoolean(prefs::kBuiltInDnsClientEnabled);

  std::string doh_mode;
  SecureDnsModeDetailsForHistogram mode_details;
  chrome_browser_net::SecureDnsUiManagementMode
      forced_management_mode_internal =
          chrome_browser_net::SecureDnsUiManagementMode::kNoOverride;
  bool is_managed =
      local_state_->FindPreference(prefs::kDnsOverHttpsMode)->IsManaged();
  if (!is_managed && ShouldDisableDohForManaged()) {
    doh_mode = chrome_browser_net::kDnsOverHttpsModeOff;
    forced_management_mode_internal =
        chrome_browser_net::SecureDnsUiManagementMode::kDisabledManaged;
  } else if (!is_managed && ShouldDisableDohForParentalControls()) {
    doh_mode = chrome_browser_net::kDnsOverHttpsModeOff;
    forced_management_mode_internal =
        chrome_browser_net::SecureDnsUiManagementMode::
            kDisabledParentalControls;
  } else {
    doh_mode = local_state_->GetString(prefs::kDnsOverHttpsMode);
  }

  if (doh_mode == chrome_browser_net::kDnsOverHttpsModeSecure) {
    *secure_dns_mode = net::DnsConfig::SecureDnsMode::SECURE;
    mode_details =
        is_managed ? SecureDnsModeDetailsForHistogram::kSecureByEnterprisePolicy
            : SecureDnsModeDetailsForHistogram::kSecureByUser;
  } else if (doh_mode == chrome_browser_net::kDnsOverHttpsModeAutomatic) {
    *secure_dns_mode = net::DnsConfig::SecureDnsMode::AUTOMATIC;
    mode_details =
        is_managed
            ? SecureDnsModeDetailsForHistogram::kAutomaticByEnterprisePolicy
            : SecureDnsModeDetailsForHistogram::kAutomaticByUser;
  } else {
    *secure_dns_mode = net::DnsConfig::SecureDnsMode::OFF;
    switch (forced_management_mode_internal) {
      case chrome_browser_net::SecureDnsUiManagementMode::kNoOverride:
        mode_details =
            is_managed
                ? SecureDnsModeDetailsForHistogram::kOffByEnterprisePolicy
                : SecureDnsModeDetailsForHistogram::kOffByUser;
        break;
      case chrome_browser_net::SecureDnsUiManagementMode::kDisabledManaged:
        mode_details =
            SecureDnsModeDetailsForHistogram::kOffByDetectedManagedEnvironment;
        break;
      case chrome_browser_net::SecureDnsUiManagementMode::
          kDisabledParentalControls:
        mode_details =
            SecureDnsModeDetailsForHistogram::kOffByDetectedParentalControls;
        break;
      default:
        NOTREACHED();
    }
  }

  if (forced_management_mode)
    *forced_management_mode = forced_management_mode_internal;

  if (record_metrics) {
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsConfig.SecureDnsMode", mode_details);
  }

  std::string doh_templates =
      local_state_->GetString(prefs::kDnsOverHttpsTemplates);
  std::string server_method;
  if (!doh_templates.empty() &&
      *secure_dns_mode != net::DnsConfig::SecureDnsMode::OFF) {
    for (base::StringPiece server_template :
    chrome_browser_net::SplitDohTemplateGroup(doh_templates)) {
      if (!net::dns_util::IsValidDohTemplate(server_template, &server_method)) {
        continue;
      }

      if (!dns_over_https_servers->has_value()) {
        *dns_over_https_servers = base::make_optional<
            std::vector<network::mojom::DnsOverHttpsServerPtr>>();
      }

      network::mojom::DnsOverHttpsServerPtr dns_over_https_server =
          network::mojom::DnsOverHttpsServer::New();
      dns_over_https_server->server_template = std::string(server_template);
      dns_over_https_server->use_post = (server_method == "POST");
      (*dns_over_https_servers)->emplace_back(std::move(dns_over_https_server));
    }
  }
}

bool StubResolverConfigReader::ShouldDisableDohForManaged() {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  if (g_browser_process->browser_policy_connector()->HasMachineLevelPolicies())
    return true;
#endif
#if defined(OS_WIN)
  if (base::IsMachineExternallyManaged())
    return true;
#endif
  return false;
}

bool StubResolverConfigReader::ShouldDisableDohForParentalControls() {
#if defined(OS_WIN)
  return ShouldDisableDohForWindowsParentalControls();
#endif

  return false;
}
