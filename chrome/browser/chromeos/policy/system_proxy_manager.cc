// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_proxy_manager.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"

namespace {
const char kSystemProxyService[] = "system-proxy-service";
}

namespace policy {

SystemProxyManager::SystemProxyManager(chromeos::CrosSettings* cros_settings)
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

  // Fire it once so we're sure we get an invocation on startup.
  OnSystemProxySettingsPolicyChanged();
}

SystemProxyManager::~SystemProxyManager() {}

std::string SystemProxyManager::SystemServicesProxyPacString() const {
  return system_proxy_enabled_ && !system_services_address_.empty()
             ? "PROXY " + system_services_address_
             : std::string();
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

  system_proxy::SetSystemTrafficCredentialsRequest request;
  request.set_system_services_username(*username);
  request.set_system_services_password(*password);

  chromeos::SystemProxyClient::Get()->SetSystemTrafficCredentials(
      request,
      base::BindOnce(&SystemProxyManager::OnSetSystemTrafficCredentials,
                     weak_factory_.GetWeakPtr()));
}

void SystemProxyManager::SetSystemServicesProxyUrlForTest(
    const std::string& local_proxy_url) {
  system_proxy_enabled_ = true;
  system_services_address_ = local_proxy_url;
}

void SystemProxyManager::OnSetSystemTrafficCredentials(
    const system_proxy::SetSystemTrafficCredentialsResponse& response) {
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
