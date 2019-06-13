// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"

#include <memory>

#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/identity_manager_wrapper.h"
#include "components/signin/core/browser/primary_account_manager.h"
#include "components/signin/core/browser/primary_account_policy_manager_impl.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"
#include "ios/web_view/internal/signin/web_view_profile_oauth2_token_service_ios_provider_impl.h"
#include "ios/web_view/internal/signin/web_view_signin_client_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "services/identity/public/cpp/accounts_cookie_mutator_impl.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/diagnostics_provider_impl.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

namespace {

std::unique_ptr<ProfileOAuth2TokenService> BuildTokenService(
    WebViewBrowserState* browser_state,
    AccountTrackerService* account_tracker_service) {
  IOSWebViewSigninClient* signin_client =
      WebViewSigninClientFactory::GetForBrowserState(browser_state);
  auto token_service_provider =
      std::make_unique<WebViewProfileOAuth2TokenServiceIOSProviderImpl>(
          signin_client);
  auto delegate = std::make_unique<ProfileOAuth2TokenServiceIOSDelegate>(
      signin_client, std::move(token_service_provider),
      account_tracker_service);
  return std::make_unique<ProfileOAuth2TokenService>(browser_state->GetPrefs(),
                                                     std::move(delegate));
}

std::unique_ptr<AccountTrackerService> BuildAccountTrackerService(
    WebViewBrowserState* browser_state) {
  auto account_tracker_service = std::make_unique<AccountTrackerService>();
  account_tracker_service->Initialize(browser_state->GetPrefs(),
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
    WebViewBrowserState* browser_state,
    AccountTrackerService* account_tracker_service,
    ProfileOAuth2TokenService* token_service) {
  // Clearing the sign in state on start up greatly simplifies the management of
  // ChromeWebView's signin state.
  PrefService* pref_service = browser_state->GetPrefs();
  pref_service->ClearPref(prefs::kGoogleServicesAccountId);
  pref_service->ClearPref(prefs::kGoogleServicesUsername);
  pref_service->ClearPref(prefs::kGoogleServicesUserAccountId);

  SigninClient* client =
      WebViewSigninClientFactory::GetForBrowserState(browser_state);
  std::unique_ptr<PrimaryAccountManager> service =
      std::make_unique<PrimaryAccountManager>(
          client, token_service, account_tracker_service,
          signin::AccountConsistencyMethod::kDisabled,
          std::make_unique<PrimaryAccountPolicyManagerImpl>(client));
  service->Initialize(ApplicationContext::GetInstance()->GetLocalState());
  return service;
}
}  // namespace

void WebViewIdentityManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  identity::IdentityManager::RegisterProfilePrefs(registry);
}

WebViewIdentityManagerFactory::WebViewIdentityManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "IdentityManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewSigninClientFactory::GetInstance());
}

WebViewIdentityManagerFactory::~WebViewIdentityManagerFactory() {}

// static
identity::IdentityManager* WebViewIdentityManagerFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewIdentityManagerFactory* WebViewIdentityManagerFactory::GetInstance() {
  static base::NoDestructor<WebViewIdentityManagerFactory> instance;
  return instance.get();
}

// static
void WebViewIdentityManagerFactory::EnsureFactoryAndDependeeFactoriesBuilt() {
  WebViewIdentityManagerFactory::GetInstance();
  WebViewSigninClientFactory::GetInstance();
}

std::unique_ptr<KeyedService>
WebViewIdentityManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  // Construct the dependencies that IdentityManager will own.
  std::unique_ptr<AccountTrackerService> account_tracker_service =
      BuildAccountTrackerService(browser_state);

  std::unique_ptr<ProfileOAuth2TokenService> token_service =
      BuildTokenService(browser_state, account_tracker_service.get());

  auto gaia_cookie_manager_service = std::make_unique<GaiaCookieManagerService>(
      token_service.get(),
      WebViewSigninClientFactory::GetForBrowserState(browser_state));

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
          WebViewSigninClientFactory::GetForBrowserState(browser_state),
          token_service.get(), account_tracker_service.get());

  return std::make_unique<IdentityManagerWrapper>(
      std::move(account_tracker_service), std::move(token_service),
      std::move(gaia_cookie_manager_service),
      std::move(primary_account_manager), std::move(account_fetcher_service),
      std::move(primary_account_mutator),
      /*accounts_mutator=*/nullptr, std::move(accounts_cookie_mutator),
      std::move(diagnostics_provider));
}

}  // namespace ios_web_view
