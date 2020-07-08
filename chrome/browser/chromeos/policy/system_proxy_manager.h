// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/dbus/system_proxy/system_proxy_service.pb.h"
#include "net/base/auth.h"

namespace system_proxy {
class SetAuthenticationDetailsResponse;
class ShutDownResponse;
}  // namespace system_proxy

class PrefService;
class PrefChangeRegistrar;
class Profile;

namespace policy {

// This class observes the device setting |SystemProxySettings|, and controls
// the availability of System-proxy service and the configuration of the web
// proxy credentials for system services connecting through System-proxy. It
// also listens for the |WorkerActive| dbus signal sent by the System-proxy
// daemon and stores connection information regarding the active worker
// processes.
class SystemProxyManager {
 public:
  SystemProxyManager(chromeos::CrosSettings* cros_settings,
                     PrefService* local_state);
  SystemProxyManager(const SystemProxyManager&) = delete;

  SystemProxyManager& operator=(const SystemProxyManager&) = delete;

  ~SystemProxyManager();

  // If System-proxy is enabled by policy, it returns the URL of the local proxy
  // instance that authenticates system services, in PAC format, e.g.
  //     PROXY localhost:3128
  // otherwise it returns an empty string.
  std::string SystemServicesProxyPacString() const;
  void StartObservingPrimaryProfilePrefs(Profile* profile);
  void StopObservingPrimaryProfilePrefs();

  void SetSystemServicesProxyUrlForTest(const std::string& local_proxy_url);

 private:
  void OnSetAuthenticationDetails(
      const system_proxy::SetAuthenticationDetailsResponse& response);
  void OnDaemonShutDown(const system_proxy::ShutDownResponse& response);

  void OnKerberosEnabledChanged();
  void OnKerberosAccountChanged();

  void SendKerberosAuthenticationDetails();

  // Once a trusted set of policies is established, this function calls
  // the System-proxy dbus client to start/shutdown the daemon and, if
  // necessary, to configure the web proxy credentials for system services.
  void OnSystemProxySettingsPolicyChanged();

  // This function is called when the |WorkerActive| dbus signal is received.
  void OnWorkerActive(const system_proxy::WorkerActiveSignalDetails& details);

  // This function is called when the |AuthenticationRequired| dbus signal is
  // received.
  void OnAuthenticationRequired(
      const system_proxy::AuthenticationRequiredDetails& details);

  // Forwards the user credentials to System-proxy. |credentials| may be empty
  // indicating the credentials for the specified |protection_space| are not
  // available.
  void LookupProxyAuthCredentialsCallback(
      const system_proxy::ProtectionSpace& protection_space,
      const base::Optional<net::AuthCredentials>& credentials);

  chromeos::CrosSettings* cros_settings_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      system_proxy_subscription_;

  bool system_proxy_enabled_ = false;
  // The authority URI in the format host:port of the local proxy worker for
  // system services.
  std::string system_services_address_;

  // Local state prefs, not owned.
  PrefService* local_state_ = nullptr;

  // Primary profile, not owned.
  Profile* primary_profile_ = nullptr;

  // Observer for Kerberos-related prefs.
  std::unique_ptr<PrefChangeRegistrar> local_state_pref_change_registrar_;
  std::unique_ptr<PrefChangeRegistrar> profile_pref_change_registrar_;

  base::WeakPtrFactory<SystemProxyManager> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_MANAGER_H_
