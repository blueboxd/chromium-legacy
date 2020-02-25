// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_SETTINGS_POLICY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_SETTINGS_POLICY_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace system_proxy {
class SetSystemTrafficCredentialsResponse;
class ShutDownResponse;
}  // namespace system_proxy

namespace policy {

// This class observes the device setting |SystemProxySettings|, and controls
// the availability of System-proxy service and the configuration of the web
// proxy credentials for system services connecting through System-proxy.
class SystemProxySettingsPolicyHandler {
 public:
  explicit SystemProxySettingsPolicyHandler(
      chromeos::CrosSettings* cros_settings);
  SystemProxySettingsPolicyHandler(const SystemProxySettingsPolicyHandler&) =
      delete;

  SystemProxySettingsPolicyHandler& operator=(
      const SystemProxySettingsPolicyHandler&) = delete;

  ~SystemProxySettingsPolicyHandler();

 private:
  void OnSetSystemTrafficCredentials(
      const system_proxy::SetSystemTrafficCredentialsResponse& response);
  void OnDaemonShutDown(const system_proxy::ShutDownResponse& response);

  // Once a trusted set of policies is established, this function calls
  // the System-proxy dbus client to start/shutdown the daemon and, if
  // necessary, to configure the web proxy credentials for system services.
  void OnSystemProxySettingsPolicyChanged();

  chromeos::CrosSettings* cros_settings_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      system_proxy_subscription_;

  base::WeakPtrFactory<SystemProxySettingsPolicyHandler> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_SETTINGS_POLICY_HANDLER_H_
