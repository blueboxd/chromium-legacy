// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_manager_builder.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/primary_account_manager.h"
#include "components/signin/core/browser/primary_account_policy_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/internal/identity_manager/accounts_cookie_mutator_impl.h"
#include "components/signin/internal/identity_manager/diagnostics_provider_impl.h"
#include "components/signin/internal/identity_manager/primary_account_mutator_impl.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "components/signin/internal/identity_manager/accounts_mutator_impl.h"
#endif

#if !defined(OS_CHROMEOS)
#include "components/signin/core/browser/primary_account_policy_manager_impl.h"
#endif

namespace identity {

namespace {

std::unique_ptr<PrimaryAccountManager> BuildPrimaryAccountManager(
    SigninClient* client,
    signin::AccountConsistencyMethod account_consistency,
    AccountTrackerService* account_tracker_service,
    ProfileOAuth2TokenService* token_service,
    PrefService* local_state) {
  std::unique_ptr<PrimaryAccountManager> primary_account_manager;
  std::unique_ptr<PrimaryAccountPolicyManager> policy_manager;
#if !defined(OS_CHROMEOS)
  policy_manager = std::make_unique<PrimaryAccountPolicyManagerImpl>(client);
#endif
  primary_account_manager = std::make_unique<PrimaryAccountManager>(
      client, token_service, account_tracker_service, account_consistency,
      std::move(policy_manager));
  primary_account_manager->Initialize(local_state);
  return primary_account_manager;
}

std::unique_ptr<AccountsMutator> BuildAccountsMutator(
    PrefService* prefs,
    AccountTrackerService* account_tracker_service,
    ProfileOAuth2TokenService* token_service,
    PrimaryAccountManager* primary_account_manager) {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  return std::make_unique<AccountsMutatorImpl>(
      token_service, account_tracker_service, primary_account_manager, prefs);
#else
  return nullptr;
#endif
}

std::unique_ptr<AccountFetcherService> BuildAccountFetcherService(
    SigninClient* signin_client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder) {
  auto account_fetcher_service = std::make_unique<AccountFetcherService>();
  account_fetcher_service->Initialize(signin_client, token_service,
                                      account_tracker_service,
                                      std::move(image_decoder));
  return account_fetcher_service;
}

}  // anonymous namespace

IdentityManagerBuildParams::IdentityManagerBuildParams() = default;

IdentityManagerBuildParams::~IdentityManagerBuildParams() = default;

std::unique_ptr<IdentityManager> BuildIdentityManager(
    IdentityManagerBuildParams* params) {
  std::unique_ptr<AccountTrackerService> account_tracker_service =
      std::move(params->account_tracker_service);

  std::unique_ptr<ProfileOAuth2TokenService> token_service =
      std::move(params->token_service);

  auto gaia_cookie_manager_service = std::make_unique<GaiaCookieManagerService>(
      token_service.get(), params->signin_client);

  std::unique_ptr<PrimaryAccountManager> primary_account_manager =
      BuildPrimaryAccountManager(params->signin_client,
                                 params->account_consistency,
                                 account_tracker_service.get(),
                                 token_service.get(), params->local_state);

  auto primary_account_mutator =
      std::make_unique<identity::PrimaryAccountMutatorImpl>(
          account_tracker_service.get(), primary_account_manager.get(),
          params->pref_service);

  std::unique_ptr<identity::AccountsMutator> accounts_mutator =
      BuildAccountsMutator(params->pref_service, account_tracker_service.get(),
                           token_service.get(), primary_account_manager.get());

  auto accounts_cookie_mutator = std::make_unique<AccountsCookieMutatorImpl>(
      gaia_cookie_manager_service.get(), account_tracker_service.get());

  auto diagnostics_provider = std::make_unique<DiagnosticsProviderImpl>(
      token_service.get(), gaia_cookie_manager_service.get());

  std::unique_ptr<AccountFetcherService> account_fetcher_service =
      BuildAccountFetcherService(params->signin_client, token_service.get(),
                                 account_tracker_service.get(),
                                 std::move(params->image_decoder));

  return std::make_unique<IdentityManager>(
      std::move(account_tracker_service), std::move(token_service),
      std::move(gaia_cookie_manager_service),
      std::move(primary_account_manager), std::move(account_fetcher_service),
      std::move(primary_account_mutator), std::move(accounts_mutator),
      std::move(accounts_cookie_mutator), std::move(diagnostics_provider));
}

}  // namespace identity
