// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_oauth2_token_service_builder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/device_id_helper.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"

#if defined(OS_ANDROID)
#include "components/signin/core/browser/oauth2_token_service_delegate_android.h"
#else
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#include "components/signin/core/browser/mutable_profile_oauth2_token_service_delegate.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service_delegate_chromeos.h"
#include "components/user_manager/user_manager.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include "chrome/browser/signin/signin_util_win.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
std::unique_ptr<signin::ProfileOAuth2TokenServiceDelegateChromeOS>
CreateCrOsOAuthDelegate(
    Profile* profile,
    AccountTrackerService* account_tracker_service,
    network::NetworkConnectionTracker* network_connection_tracker) {
  chromeos::AccountManagerFactory* factory =
      g_browser_process->platform_part()->GetAccountManagerFactory();
  DCHECK(factory);
  chromeos::AccountManager* account_manager =
      factory->GetAccountManager(profile->GetPath().value());
  DCHECK(account_manager);

  const bool is_regular_profile =
      !chromeos::ProfileHelper::IsSigninProfile(profile) &&
      !chromeos::ProfileHelper::IsLockScreenAppProfile(profile);
  return std::make_unique<signin::ProfileOAuth2TokenServiceDelegateChromeOS>(
      account_tracker_service, network_connection_tracker, account_manager,
      is_regular_profile);
}
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)

// Supervised users cannot revoke credentials.
bool CanRevokeCredentials(Profile* profile) {
#if defined(OS_CHROMEOS)
  // UserManager may not exist in unit_tests.
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser()) {
    // Don't allow revoking credentials for Chrome OS supervised users.
    // See http://crbug.com/332032
    LOG(ERROR) << "Attempt to revoke supervised user refresh "
               << "token detected, ignoring.";
    return false;
  }
#endif

  return true;
}

std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate>
CreateMutableProfileOAuthDelegate(
    Profile* profile,
    AccountTrackerService* account_tracker_service,
    signin::AccountConsistencyMethod account_consistency,
    SigninClient* signin_client,
    network::NetworkConnectionTracker* network_connection_tracker) {
  // When signin cookies are cleared on exit and Dice is enabled, all tokens
  // should also be cleared.
  bool revoke_all_tokens_on_load =
      (account_consistency == signin::AccountConsistencyMethod::kDice) &&
      signin::SettingsDeleteSigninCookiesOnExit(
          CookieSettingsFactory::GetForProfile(profile).get());

  return std::make_unique<MutableProfileOAuth2TokenServiceDelegate>(
      signin_client, account_tracker_service, network_connection_tracker,
      WebDataServiceFactory::GetTokenWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      account_consistency, revoke_all_tokens_on_load,
      CanRevokeCredentials(profile),
#if defined(OS_WIN)
      base::BindRepeating(&signin_util::ReauthWithCredentialProviderIfPossible,
                          base::Unretained(profile)));
#else
      MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback());
#endif  // defined(OS_WIN)
}
#endif  // !defined(OS_ANDROID)

std::unique_ptr<OAuth2TokenServiceDelegate> CreateOAuth2TokenServiceDelegate(
    Profile* profile,
    AccountTrackerService* account_tracker_service,
    signin::AccountConsistencyMethod account_consistency,
    SigninClient* signin_client,
    network::NetworkConnectionTracker* network_connection_tracker) {
#if defined(OS_ANDROID)
  return std::make_unique<OAuth2TokenServiceDelegateAndroid>(
      account_tracker_service);

#else  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  if (chromeos::switches::IsAccountManagerEnabled()) {
    return CreateCrOsOAuthDelegate(profile, account_tracker_service,
                                   network_connection_tracker);
  }
#endif  // defined(OS_CHROMEOS)

  // Fall back to |MutableProfileOAuth2TokenServiceDelegate|:
  // 1. On all platforms other than Android and Chrome OS.
  // 2. On Chrome OS, if Account Manager has not been switched on yet
  // (chromeos::switches::IsAccountManagerEnabled).
  return CreateMutableProfileOAuthDelegate(profile, account_tracker_service,
                                           account_consistency, signin_client,
                                           network_connection_tracker);

#endif  // defined(OS_ANDROID)
}

}  // namespace

// static
std::unique_ptr<ProfileOAuth2TokenService>
ProfileOAuth2TokenServiceBuilder::BuildInstanceFor(
    content::BrowserContext* context,
    PrefService* pref_service,
    AccountTrackerService* account_tracker_service,
    network::NetworkConnectionTracker* network_connection_tracker,
    signin::AccountConsistencyMethod account_consistency,
    SigninClient* signin_client) {
  Profile* profile = static_cast<Profile*>(context);

// On ChromeOS the device ID is not managed by the token service.
#if !defined(OS_CHROMEOS)
  // Ensure the device ID is not empty. This is important for Dice, because the
  // device ID is needed on the network thread, but can only be generated on the
  // main thread.
  std::string device_id = signin::GetSigninScopedDeviceId(pref_service);
  DCHECK(!device_id.empty());
#endif

  return std::make_unique<ProfileOAuth2TokenService>(
      pref_service, CreateOAuth2TokenServiceDelegate(
                        profile, account_tracker_service, account_consistency,
                        signin_client, network_connection_tracker));
}
