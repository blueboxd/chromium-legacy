// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_proxy_manager.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "net/http/http_auth_scheme.h"

namespace {
const char kSystemProxyService[] = "system-proxy-service";
}  // namespace

namespace policy {

SystemProxyManager::SystemProxyManager(chromeos::CrosSettings* cros_settings,
                                       PrefService* local_state)
    : cros_settings_(cros_settings),
      system_proxy_subscription_(cros_settings_->AddSettingsObserver(
          chromeos::kSystemProxySettings,
          base::BindRepeating(
              &SystemProxyManager::OnSystemProxySettingsPolicyChanged,
              base::Unretained(this)))) {
  // Connect to System-proxy signals.
  chromeos::SystemProxyClient::Get()->SetWorkerActiveSignalCallback(
      base::BindRepeating(&SystemProxyManager::OnWorkerActive,
                          weak_factory_.GetWeakPtr()));
  chromeos::SystemProxyClient::Get()->SetAuthenticationRequiredSignalCallback(
      base::BindRepeating(&SystemProxyManager::OnAuthenticationRequired,
                          weak_factory_.GetWeakPtr()));
  chromeos::SystemProxyClient::Get()->ConnectToWorkerSignals();
  local_state_ = local_state;

  // Listen to pref changes.
  local_state_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  local_state_pref_change_registrar_->Init(local_state_);
  local_state_pref_change_registrar_->Add(
      prefs::kKerberosEnabled,
      base::BindRepeating(&SystemProxyManager::OnKerberosEnabledChanged,
                          weak_factory_.GetWeakPtr()));

  // Fire it once so we're sure we get an invocation on startup.
  OnSystemProxySettingsPolicyChanged();
}

SystemProxyManager::~SystemProxyManager() = default;

std::string SystemProxyManager::SystemServicesProxyPacString() const {
  return system_proxy_enabled_ && !system_services_address_.empty()
             ? "PROXY " + system_services_address_
             : std::string();
}

void SystemProxyManager::StartObservingPrimaryProfilePrefs(Profile* profile) {
  primary_profile_ = profile;

  // Listen to pref changes.
  profile_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  profile_pref_change_registrar_->Init(primary_profile_->GetPrefs());
  profile_pref_change_registrar_->Add(
      prefs::kKerberosActivePrincipalName,
      base::BindRepeating(&SystemProxyManager::OnKerberosAccountChanged,
                          base::Unretained(this)));
  if (system_proxy_enabled_) {
    OnKerberosAccountChanged();
  }
}

void SystemProxyManager::StopObservingPrimaryProfilePrefs() {
  profile_pref_change_registrar_->RemoveAll();
  profile_pref_change_registrar_.reset();
}

void SystemProxyManager::OnSystemProxySettingsPolicyChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(base::BindOnce(
          &SystemProxyManager::OnSystemProxySettingsPolicyChanged,
          base::Unretained(this)));
  if (status != chromeos::CrosSettingsProvider::TRUSTED)
    return;

  const base::Value* proxy_settings =
      cros_settings_->GetPref(chromeos::kSystemProxySettings);

  if (!proxy_settings)
    return;

  system_proxy_enabled_ =
      proxy_settings->FindBoolKey(chromeos::kSystemProxySettingsKeyEnabled)
          .value_or(false);
  // System-proxy is inactive by default.
  if (!system_proxy_enabled_) {
    // Send a shut-down command to the daemon. Since System-proxy is started via
    // dbus activation, if the daemon is inactive, this command will start the
    // daemon and tell it to exit.
    // TODO(crbug.com/1055245,acostinas): Do not send shut-down command if
    // System-proxy is inactive.
    chromeos::SystemProxyClient::Get()->ShutDownDaemon(base::BindOnce(
        &SystemProxyManager::OnDaemonShutDown, weak_factory_.GetWeakPtr()));
    system_services_address_.clear();
    return;
  }

  system_proxy::SetAuthenticationDetailsRequest request;
  system_proxy::Credentials credentials;
  const std::string* username = proxy_settings->FindStringKey(
      chromeos::kSystemProxySettingsKeySystemServicesUsername);

  const std::string* password = proxy_settings->FindStringKey(
      chromeos::kSystemProxySettingsKeySystemServicesPassword);

  if (!username || username->empty() || !password || password->empty()) {
    NET_LOG(DEBUG) << "Proxy credentials for system traffic not set: "
                   << kSystemProxyService;
  } else {
    credentials.set_username(*username);
    credentials.set_password(*password);
    *request.mutable_credentials() = credentials;
  }

  request.set_traffic_type(system_proxy::TrafficOrigin::SYSTEM);

  chromeos::SystemProxyClient::Get()->SetAuthenticationDetails(
      request, base::BindOnce(&SystemProxyManager::OnSetAuthenticationDetails,
                              weak_factory_.GetWeakPtr()));
}

void SystemProxyManager::OnKerberosEnabledChanged() {
  SendKerberosAuthenticationDetails();
}

void SystemProxyManager::OnKerberosAccountChanged() {
  if (!local_state_->GetBoolean(prefs::kKerberosEnabled)) {
    return;
  }
  SendKerberosAuthenticationDetails();
}

void SystemProxyManager::SendKerberosAuthenticationDetails() {
  if (!system_proxy_enabled_) {
    return;
  }

  system_proxy::SetAuthenticationDetailsRequest request;
  request.set_traffic_type(system_proxy::TrafficOrigin::SYSTEM);
  request.set_kerberos_enabled(
      local_state_->GetBoolean(prefs::kKerberosEnabled));
  if (primary_profile_) {
    request.set_active_principal_name(
        primary_profile_->GetPrefs()
            ->Get(prefs::kKerberosActivePrincipalName)
            ->GetString());
  }
  chromeos::SystemProxyClient::Get()->SetAuthenticationDetails(
      request, base::BindOnce(&SystemProxyManager::OnSetAuthenticationDetails,
                              weak_factory_.GetWeakPtr()));
}

void SystemProxyManager::SetSystemServicesProxyUrlForTest(
    const std::string& local_proxy_url) {
  system_proxy_enabled_ = true;
  system_services_address_ = local_proxy_url;
}

void SystemProxyManager::OnSetAuthenticationDetails(
    const system_proxy::SetAuthenticationDetailsResponse& response) {
  if (response.has_error_message()) {
    NET_LOG(ERROR)
        << "Failed to set system traffic credentials for system proxy: "
        << kSystemProxyService << ", Error: " << response.error_message();
  }
}

void SystemProxyManager::OnDaemonShutDown(
    const system_proxy::ShutDownResponse& response) {
  if (response.has_error_message() && !response.error_message().empty()) {
    NET_LOG(ERROR) << "Failed to shutdown system proxy: " << kSystemProxyService
                   << ", error: " << response.error_message();
  }
}

void SystemProxyManager::OnWorkerActive(
    const system_proxy::WorkerActiveSignalDetails& details) {
  if (details.traffic_origin() == system_proxy::TrafficOrigin::SYSTEM) {
    system_services_address_ = details.local_proxy_url();
  }
}

void SystemProxyManager::OnAuthenticationRequired(
    const system_proxy::AuthenticationRequiredDetails& details) {
  system_proxy::ProtectionSpace protection_space =
      details.proxy_protection_space();

  // TODO(acostinas, crbug.com/1098216): Get credentials from the network
  // service.
  LookupProxyAuthCredentialsCallback(protection_space,
                                     /* credentials = */ base::nullopt);
}

void SystemProxyManager::LookupProxyAuthCredentialsCallback(
    const system_proxy::ProtectionSpace& protection_space,
    const base::Optional<net::AuthCredentials>& credentials) {
  // System-proxy is started via d-bus activation, meaning the first d-bus call
  // will start the daemon. Check that System-proxy was not disabled by policy
  // while looking for credentials so we don't accidentally restart it.
  if (!system_proxy_enabled_) {
    return;
  }
  std::string username;
  std::string password;
  if (credentials) {
    username = base::UTF16ToUTF8(credentials->username());
    password = base::UTF16ToUTF8(credentials->password());
  }

  system_proxy::Credentials user_credentials;
  user_credentials.set_username(username);
  user_credentials.set_password(password);

  system_proxy::SetAuthenticationDetailsRequest request;
  request.set_traffic_type(system_proxy::TrafficOrigin::SYSTEM);
  *request.mutable_credentials() = user_credentials;
  *request.mutable_protection_space() = protection_space;

  chromeos::SystemProxyClient::Get()->SetAuthenticationDetails(
      request, base::BindOnce(&SystemProxyManager::OnSetAuthenticationDetails,
                              weak_factory_.GetWeakPtr()));
}

}  // namespace policy
