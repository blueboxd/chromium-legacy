// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_
#define CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "net/net_buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"

class Profile;
class TrialComparisonCertVerifierController;

namespace user_prefs {
class PrefRegistrySyncable;
}

// KeyedService that initializes and provides access to the NetworkContexts for
// a Profile. This will eventually replace ProfileIOData.
class ProfileNetworkContextService
    : public KeyedService,
      public content_settings::Observer,
      public content_settings::CookieSettings::Observer {
 public:
  explicit ProfileNetworkContextService(Profile* profile);
  ~ProfileNetworkContextService() override;

  // Creates a NetworkContext for the BrowserContext, using the specified
  // parameters. An empty |relative_partition_path| corresponds to the main
  // network context.
  network::mojom::NetworkContextPtr CreateNetworkContext(
      bool in_memory,
      const base::FilePath& relative_partition_path);

#if defined(OS_CHROMEOS)
  void UpdateAdditionalCertificates(
      const net::CertificateList& all_additional_certificates,
      const net::CertificateList& trust_anchors);

  bool using_builtin_cert_verifier() { return using_builtin_cert_verifier_; }
#endif

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Packages up configuration info in |profile| and |cookie_settings| into a
  // mojo-friendly form.
  static network::mojom::CookieManagerParamsPtr CreateCookieManagerParams(
      Profile* profile,
      const content_settings::CookieSettings& cookie_settings);

  // Flushes all pending proxy configuration changes.
  void FlushProxyConfigMonitorForTesting();

  static void SetDiscardDomainReliabilityUploadsForTesting(bool value);

 private:
  // Checks |quic_allowed_|, and disables QUIC if needed.
  void DisableQuicIfNotAllowed();

  // Forwards changes to |pref_accept_language_| to the NetworkContext, after
  // formatting them as appropriate.
  void UpdateAcceptLanguage();

  // Forwards changes to |block_third_party_cookies_| to the NetworkContext.
  void UpdateBlockThirdPartyCookies();

  // Computes appropriate value of Accept-Language header based on
  // |pref_accept_language_|
  std::string ComputeAcceptLanguage() const;

  void UpdateReferrersEnabled();

  // Update the CTPolicy for the given NetworkContexts.
  void UpdateCTPolicyForContexts(
      const std::vector<network::mojom::NetworkContext*>& contexts);

  // Update the CTPolicy for the all of profiles_'s NetworkContexts.
  void UpdateCTPolicy();

  void ScheduleUpdateCTPolicy();

  // Creates parameters for the NetworkContext. Use |in_memory| instead of
  // |profile_->IsOffTheRecord()| because sometimes normal profiles want off the
  // record partitions (e.g. for webview tag).
  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams(
      bool in_memory,
      const base::FilePath& relative_partition_path);

  // Returns the path for a given storage partition.
  base::FilePath GetPartitionPath(
      const base::FilePath& relative_partition_path);

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  // content_settings::CookieSettings::Observer:
  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override;

  Profile* const profile_;

  ProxyConfigMonitor proxy_config_monitor_;

  BooleanPrefMember quic_allowed_;
  StringPrefMember pref_accept_language_;
  BooleanPrefMember enable_referrers_;
  PrefChangeRegistrar pref_change_registrar_;

  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  ScopedObserver<content_settings::CookieSettings, ProfileNetworkContextService>
      cookie_settings_observer_;

  // Used to post schedule CT policy updates
  base::OneShotTimer ct_policy_update_timer_;

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
  // Controls the cert verification trial. May be null if the trial is disabled
  // or not allowed for this profile.
  std::unique_ptr<TrialComparisonCertVerifierController>
      trial_comparison_cert_verifier_controller_;
#endif

#if defined(OS_CHROMEOS)
  bool using_builtin_cert_verifier_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ProfileNetworkContextService);
};

#endif  // CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_
