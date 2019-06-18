// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/identity_manager_factory.h"

#include <memory>
#include <utility>

#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/identity_manager_wrapper.h"
#include "components/signin/core/browser/primary_account_manager.h"
#include "components/signin/core/browser/primary_account_policy_manager_impl.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory_observer.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_ios_provider_impl.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"
#include "services/identity/public/cpp/accounts_cookie_mutator_impl.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/diagnostics_provider_impl.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator_impl.h"

namespace {

std::unique_ptr<ProfileOAuth2TokenService> BuildTokenService(
    ios::ChromeBrowserState* chrome_browser_state,
    AccountTrackerService* account_tracker_service) {
  auto delegate = std::make_unique<ProfileOAuth2TokenServiceIOSDelegate>(
      SigninClientFactory::GetForBrowserState(chrome_browser_state),
      std::make_unique<ProfileOAuth2TokenServiceIOSProviderImpl>(),
      account_tracker_service);
  return std::make_unique<ProfileOAuth2TokenService>(
      chrome_browser_state->GetPrefs(), std::move(delegate));
}

std::unique_ptr<AccountTrackerService> BuildAccountTrackerService(
    ios::ChromeBrowserState* chrome_browser_state) {
  auto account_tracker_service = std::make_unique<AccountTrackerService>();
  account_tracker_service->Initialize(chrome_browser_state->GetPrefs(),
                                      base::FilePath());
  return account_tracker_service;
}

std::unique_ptr<AccountFetcherService> BuildAccountFetcherService(
    SigninClient* signin_client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service) {
  auto account_fetcher_service = std::make_unique<AccountFetcherService>();
  account_fetcher_service->Initialize(signin_client, token_service,
                                      account_tracker_service,
                                      image_fetcher::CreateIOSImageDecoder());
  return account_fetcher_service;
}

std::unique_ptr<PrimaryAccountManager> BuildPrimaryAccountManager(
    ios::ChromeBrowserState* chrome_browser_state,
    AccountTrackerService* account_tracker_service,
    ProfileOAuth2TokenService* token_service) {
  SigninClient* client =
      SigninClientFactory::GetForBrowserState(chrome_browser_state);
  std::unique_ptr<PrimaryAccountManager> service =
      std::make_unique<PrimaryAccountManager>(
          client, token_service, account_tracker_service,
          signin::AccountConsistencyMethod::kMirror,
          std::make_unique<PrimaryAccountPolicyManagerImpl>(client));
  service->Initialize(GetApplicationContext()->GetLocalState());
  return service;
}

}  // namespace

void IdentityManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  identity::IdentityManager::RegisterProfilePrefs(registry);
}

IdentityManagerFactory::IdentityManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "IdentityManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SigninClientFactory::GetInstance());
}

IdentityManagerFactory::~IdentityManagerFactory() {}

// static
identity::IdentityManager* IdentityManagerFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
identity::IdentityManager* IdentityManagerFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
IdentityManagerFactory* IdentityManagerFactory::GetInstance() {
  static base::NoDestructor<IdentityManagerFactory> instance;
  return instance.get();
}

// static
void IdentityManagerFactory::EnsureFactoryAndDependeeFactoriesBuilt() {
  IdentityManagerFactory::GetInstance();
  SigninClientFactory::GetInstance();
}

void IdentityManagerFactory::AddObserver(
    IdentityManagerFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManagerFactory::RemoveObserver(
    IdentityManagerFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

std::unique_ptr<KeyedService> IdentityManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);

  // Construct the dependencies that IdentityManager will own.
  std::unique_ptr<AccountTrackerService> account_tracker_service =
      BuildAccountTrackerService(browser_state);

  std::unique_ptr<ProfileOAuth2TokenService> token_service =
      BuildTokenService(browser_state, account_tracker_service.get());

  auto gaia_cookie_manager_service = std::make_unique<GaiaCookieManagerService>(
      token_service.get(),
      SigninClientFactory::GetForBrowserState(browser_state));

  std::unique_ptr<PrimaryAccountManager> primary_account_manager =
      BuildPrimaryAccountManager(browser_state, account_tracker_service.get(),
                                 token_service.get());

  auto primary_account_mutator =
      std::make_unique<identity::PrimaryAccountMutatorImpl>(
          account_tracker_service.get(), primary_account_manager.get(),
          browser_state->GetPrefs());

  auto accounts_cookie_mutator =
      std::make_unique<identity::AccountsCookieMutatorImpl>(
          gaia_cookie_manager_service.get(), account_tracker_service.get());

  auto diagnostics_provider =
      std::make_unique<identity::DiagnosticsProviderImpl>(
          token_service.get(), gaia_cookie_manager_service.get());

  std::unique_ptr<AccountFetcherService> account_fetcher_service =
      BuildAccountFetcherService(
          SigninClientFactory::GetForBrowserState(browser_state),
          token_service.get(), account_tracker_service.get());

  auto identity_manager = std::make_unique<IdentityManagerWrapper>(
      std::move(account_tracker_service), std::move(token_service),
      std::move(gaia_cookie_manager_service),
      std::move(primary_account_manager), std::move(account_fetcher_service),
      std::move(primary_account_mutator),
      /*accounts_mutator=*/nullptr, std::move(accounts_cookie_mutator),
      std::move(diagnostics_provider));

  for (auto& observer : observer_list_)
    observer.IdentityManagerCreated(identity_manager.get());

  return identity_manager;
}

void IdentityManagerFactory::BrowserStateShutdown(web::BrowserState* context) {
  auto* identity_manager = static_cast<IdentityManagerWrapper*>(
      GetServiceForBrowserState(context, false));
  if (identity_manager) {
    for (auto& observer : observer_list_)
      observer.IdentityManagerShutdown(identity_manager);
  }
  BrowserStateKeyedServiceFactory::BrowserStateShutdown(context);
}
