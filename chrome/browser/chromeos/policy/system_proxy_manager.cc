// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_proxy_manager.h"

#include "base/bind.h"
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

namespace {
const char kSystemProxyService[] = "system-proxy-service";
}

namespace policy {

SystemProxyManager::SystemProxyManager(chromeos::CrosSettings* cros_settings,
                                       PrefService* local_state)
    : cros_settings_(cros_settings),
      system_proxy_subscription_(cros_settings_->AddSettingsObserver(
          chromeos::kSystemProxySettings,
          base::BindRepeating(
              &SystemProxyManager::OnSystemProxySettingsPolicyChanged,
              base::Unretained(this)))) {
  // Connect to a signal that indicates when a worker process is active.
  chromeos::SystemProxyClient::Get()->ConnectToWorkerActiveSignal(
      base::BindRepeating(&SystemProxyManager::OnWorkerActive,
                          weak_factory_.GetWeakPtr()));
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

  const std::string* username = proxy_settings->FindStringKey(
      chromeos::kSystemProxySettingsKeySystemServicesUsername);

  const std::string* password = proxy_settings->FindStringKey(
      chromeos::kSystemProxySettingsKeySystemServicesPassword);

  if (!username || username->empty() || !password || password->empty()) {
    NET_LOG(ERROR) << "Proxy credentials for system traffic not set: "
                   << kSystemProxyService;
    return;
  }

  system_proxy::Credentials credentials;
  credentials.set_username(*username);
  credentials.set_password(*password);

  system_proxy::SetAuthenticationDetailsRequest request;
  request.set_traffic_type(system_proxy::TrafficOrigin::SYSTEM);
  *request.mutable_credentials() = credentials;

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

}  // namespace policy
