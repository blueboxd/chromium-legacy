// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_manager_factory.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_builder.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/identity_manager_wrapper.h"
#include "components/signin/core/browser/primary_account_manager.h"
#include "components/signin/core/browser/primary_account_policy_manager_impl.h"
#include "components/signin/core/browser/signin_client.h"
#include "services/identity/public/cpp/accounts_cookie_mutator.h"
#include "services/identity/public/cpp/accounts_cookie_mutator_impl.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/diagnostics_provider_impl.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator.h"

#if !defined(OS_CHROMEOS)
#include "services/identity/public/cpp/primary_account_mutator_impl.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/web_data_service_factory.h"
#include "components/signin/core/browser/mutable_profile_oauth2_token_service_delegate.h"
#include "services/identity/public/cpp/accounts_mutator_impl.h"
#endif

namespace {

// Helper function returning a newly constructed PrimaryAccountMutator for
// |profile|.  May return null if mutation of the signed-in state is not
// supported on the current platform.
std::unique_ptr<identity::PrimaryAccountMutator> BuildPrimaryAccountMutator(
    Profile* profile,
    AccountTrackerService* account_tracker_service,
    PrimaryAccountManager* primary_account_manager) {
#if !defined(OS_CHROMEOS)
  return std::make_unique<identity::PrimaryAccountMutatorImpl>(
      account_tracker_service, primary_account_manager, profile->GetPrefs());
#else
  return nullptr;
#endif
}

// Helper function returning a newly constructed AccountsMutator for
// |profile|. May return null if mutation of accounts is not supported on the
// current platform.
std::unique_ptr<identity::AccountsMutator> BuildAccountsMutator(
    Profile* profile,
    AccountTrackerService* account_tracker_service,
    ProfileOAuth2TokenService* token_service,
    PrimaryAccountManager* primary_account_manager) {
#if !defined(OS_ANDROID)
  return std::make_unique<identity::AccountsMutatorImpl>(
      token_service, account_tracker_service, primary_account_manager,
      profile->GetPrefs());
#else
  return nullptr;
#endif
}

std::unique_ptr<PrimaryAccountManager> BuildPrimaryAccountManager(
    Profile* profile,
    AccountTrackerService* account_tracker_service,
    ProfileOAuth2TokenService* token_service) {
  std::unique_ptr<PrimaryAccountManager> primary_account_manager;
  SigninClient* client =
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile);
  std::unique_ptr<PrimaryAccountPolicyManager> policy_manager;
#if !defined(OS_CHROMEOS)
  policy_manager = std::make_unique<PrimaryAccountPolicyManagerImpl>(client);
#endif
  primary_account_manager = std::make_unique<PrimaryAccountManager>(
      client, token_service, account_tracker_service,
      AccountConsistencyModeManager::GetMethodForProfile(profile),
      std::move(policy_manager));
  primary_account_manager->Initialize(g_browser_process->local_state());
  return primary_account_manager;
}

std::unique_ptr<AccountTrackerService> BuildAccountTrackerService(
    Profile* profile) {
  auto account_tracker_service = std::make_unique<AccountTrackerService>();
  account_tracker_service->Initialize(profile->GetPrefs(), profile->GetPath());
  return account_tracker_service;
}

std::unique_ptr<AccountFetcherService> BuildAccountFetcherService(
    SigninClient* signin_client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service) {
  auto account_fetcher_service = std::make_unique<AccountFetcherService>();
  account_fetcher_service->Initialize(signin_client, token_service,
                                      account_tracker_service,
                                      std::make_unique<ImageDecoderImpl>());
  return account_fetcher_service;
}

}  // namespace

void IdentityManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  identity::IdentityManager::RegisterProfilePrefs(registry);
#if !defined(OS_ANDROID)
  MutableProfileOAuth2TokenServiceDelegate::RegisterProfilePrefs(registry);
#endif
}

IdentityManagerFactory::IdentityManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "IdentityManager",
          BrowserContextDependencyManager::GetInstance()) {
#if !defined(OS_ANDROID)
  DependsOn(WebDataServiceFactory::GetInstance());
#endif
  DependsOn(ChromeSigninClientFactory::GetInstance());
}

IdentityManagerFactory::~IdentityManagerFactory() {}

// static
identity::IdentityManager* IdentityManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
identity::IdentityManager* IdentityManagerFactory::GetForProfileIfExists(
    const Profile* profile) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserContext(const_cast<Profile*>(profile),
                                                 false));
}

// static
IdentityManagerFactory* IdentityManagerFactory::GetInstance() {
  return base::Singleton<IdentityManagerFactory>::get();
}

// static
void IdentityManagerFactory::EnsureFactoryAndDependeeFactoriesBuilt() {
  IdentityManagerFactory::GetInstance();
  ChromeSigninClientFactory::GetInstance();
}

void IdentityManagerFactory::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManagerFactory::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

KeyedService* IdentityManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // Construct the dependencies that IdentityManager will own.
  std::unique_ptr<AccountTrackerService> account_tracker_service =
      BuildAccountTrackerService(profile);

  std::unique_ptr<ProfileOAuth2TokenService> token_service =
      ProfileOAuth2TokenServiceBuilder::BuildInstanceFor(
          context, account_tracker_service.get());

  auto gaia_cookie_manager_service = std::make_unique<GaiaCookieManagerService>(
      token_service.get(), ChromeSigninClientFactory::GetForProfile(profile));

  std::unique_ptr<PrimaryAccountManager> primary_account_manager =
      BuildPrimaryAccountManager(profile, account_tracker_service.get(),
                                 token_service.get());

  std::unique_ptr<identity::PrimaryAccountMutator> primary_account_mutator =
      BuildPrimaryAccountMutator(profile, account_tracker_service.get(),
                                 primary_account_manager.get());

  std::unique_ptr<identity::AccountsMutator> accounts_mutator =
      BuildAccountsMutator(profile, account_tracker_service.get(),
                           token_service.get(), primary_account_manager.get());

  auto accounts_cookie_mutator =
      std::make_unique<identity::AccountsCookieMutatorImpl>(
          gaia_cookie_manager_service.get(), account_tracker_service.get());

  auto diagnostics_provider =
      std::make_unique<identity::DiagnosticsProviderImpl>(
          token_service.get(), gaia_cookie_manager_service.get());

  std::unique_ptr<AccountFetcherService> account_fetcher_service =
      BuildAccountFetcherService(
          ChromeSigninClientFactory::GetForProfile(profile),
          token_service.get(), account_tracker_service.get());

  auto identity_manager = std::make_unique<IdentityManagerWrapper>(
      std::move(account_tracker_service), std::move(token_service),
      std::move(gaia_cookie_manager_service),
      std::move(primary_account_manager), std::move(account_fetcher_service),
      std::move(primary_account_mutator), std::move(accounts_mutator),
      std::move(accounts_cookie_mutator), std::move(diagnostics_provider));

  for (Observer& observer : observer_list_)
    observer.IdentityManagerCreated(identity_manager.get());

  return identity_manager.release();
}

void IdentityManagerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  auto* identity_manager = static_cast<IdentityManagerWrapper*>(
      GetServiceForBrowserContext(context, false));
  if (identity_manager) {
    for (Observer& observer : observer_list_)
      observer.IdentityManagerShutdown(identity_manager);
  }
  BrowserContextKeyedServiceFactory::BrowserContextShutdown(context);
}
