// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_proxy_settings_policy_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/dbus/system_proxy/system_proxy_service.pb.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"

namespace {
const char kSystemProxyService[] = "system-proxy-service";
}

namespace policy {

SystemProxySettingsPolicyHandler::SystemProxySettingsPolicyHandler(
    chromeos::CrosSettings* cros_settings)
    : cros_settings_(cros_settings),
      system_proxy_subscription_(cros_settings_->AddSettingsObserver(
          chromeos::kSystemProxySettings,
          base::BindRepeating(&SystemProxySettingsPolicyHandler::
                                  OnSystemProxySettingsPolicyChanged,
                              base::Unretained(this)))) {
  // Fire it once so we're sure we get an invocation on startup.
  OnSystemProxySettingsPolicyChanged();
}

SystemProxySettingsPolicyHandler::~SystemProxySettingsPolicyHandler() {}

void SystemProxySettingsPolicyHandler::OnSystemProxySettingsPolicyChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(base::BindOnce(
          &SystemProxySettingsPolicyHandler::OnSystemProxySettingsPolicyChanged,
          base::Unretained(this)));
  if (status != chromeos::CrosSettingsProvider::TRUSTED)
    return;

  const base::Value* proxy_settings =
      cros_settings_->GetPref(chromeos::kSystemProxySettings);

  if (!proxy_settings)
    return;

  bool enabled =
      proxy_settings->FindBoolKey(chromeos::kSystemProxySettingsKeyEnabled)
          .value_or(false);
  // System-proxy is inactive by default.
  if (!enabled) {
    // Send a shut-down command to the daemon. Since System-proxy is started via
    // dbus activation, if the daemon is inactive, this command will start the
    // daemon and tell it to exit.
    // TODO(crbug.com/1055245,acostinas): Do not send shut-down command if
    // System-proxy is inactive.
    chromeos::SystemProxyClient::Get()->ShutDownDaemon(
        base::BindOnce(&SystemProxySettingsPolicyHandler::OnDaemonShutDown,
                       weak_factory_.GetWeakPtr()));
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
      base::BindOnce(
          &SystemProxySettingsPolicyHandler::OnSetSystemTrafficCredentials,
          weak_factory_.GetWeakPtr()));
}

void SystemProxySettingsPolicyHandler::OnSetSystemTrafficCredentials(
    const system_proxy::SetSystemTrafficCredentialsResponse& response) {
  if (response.has_error_message()) {
    NET_LOG(ERROR)
        << "Failed to set system traffic credentials for system proxy: "
        << kSystemProxyService << ", Error: " << response.error_message();
  }
}

void SystemProxySettingsPolicyHandler::OnDaemonShutDown(
    const system_proxy::ShutDownResponse& response) {
  if (response.has_error_message() && !response.error_message().empty()) {
    NET_LOG(ERROR) << "Failed to shutdown system proxy: " << kSystemProxyService
                   << ", error: " << response.error_message();
  }
}

}  // namespace policy
