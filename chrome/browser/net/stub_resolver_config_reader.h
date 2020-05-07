// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_STUB_RESOLVER_CONFIG_READER_H_
#define CHROME_BROWSER_NET_STUB_RESOLVER_CONFIG_READER_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_change_registrar.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"

class PrefRegistrySimple;
class PrefService;
class SecureDnsConfig;

// Retriever for Chrome configuration for the built-in DNS stub resolver.
class StubResolverConfigReader {
 public:
  static constexpr base::TimeDelta kParentalControlsCheckDelay =
      base::TimeDelta::FromSeconds(2);

  // |local_state| must outlive the created reader.
  explicit StubResolverConfigReader(PrefService* local_state,
                                    bool set_up_pref_defaults = true);

  StubResolverConfigReader(const StubResolverConfigReader&) = delete;
  StubResolverConfigReader& operator=(const StubResolverConfigReader&) = delete;

  virtual ~StubResolverConfigReader() = default;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the current secure DNS resolver configuration.
  //
  // Initial checks for parental controls (which cause DoH to be disabled) may
  // be deferred for performance if called early during startup, if the
  // configuration is otherwise in AUTOMATIC mode. If this is undesirable, e.g.
  // because this is being called to populate the config UI, set
  // |force_check_parental_controls_for_automatic_mode| to force always waiting
  // for the parental controls check. If forcing the check when it had
  // previously been deferred, and the check discovers that DoH should be
  // disabled, the network service will be updated to disable DoH and ensure the
  // service behavior matches the config returned by this method.
  SecureDnsConfig GetSecureDnsConfiguration(
      bool force_check_parental_controls_for_automatic_mode);

  bool GetInsecureStubResolverEnabled();

  // Updates the network service with the current configuration.
  void UpdateNetworkService(bool record_metrics);

  // Returns true if there are any active machine level policies or if the
  // machine is domain joined. This special logic is used to disable DoH by
  // default for Desktop platforms (the enterprise policy field
  // default_for_enterprise_users only applies to ChromeOS). We don't attempt
  // enterprise detection on Android at this time.
  virtual bool ShouldDisableDohForManaged();

  // Returns true if there are parental controls detected on the device.
  virtual bool ShouldDisableDohForParentalControls();

 private:
  void OnParentalControlsDelayTimer();

  // Updates network service if |update_network_service| or if necessary due to
  // first read of parental controls.
  SecureDnsConfig GetAndUpdateConfiguration(
      bool force_check_parental_controls_for_automatic_mode,
      bool record_metrics,
      bool update_network_service);

  PrefService* const local_state_;

  // Timer for deferred running of parental controls checks. Underling API calls
  // may be slow and run off-thread. Calling for the result is delayed to avoid
  // blocking during startup.
  base::OneShotTimer parental_controls_delay_timer_;
  // Whether or not parental controls have already been checked, either due to
  // expiration of the delay timer or because of a forced check.
  bool parental_controls_checked_ = false;

  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_NET_STUB_RESOLVER_CONFIG_READER_H_
