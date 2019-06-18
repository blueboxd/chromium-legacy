// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/identity_manager_wrapper.h"

#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "services/identity/public/cpp/accounts_cookie_mutator.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/diagnostics_provider.h"
#include "services/identity/public/cpp/primary_account_mutator.h"

IdentityManagerWrapper::IdentityManagerWrapper(
    std::unique_ptr<AccountTrackerService> account_tracker_service,
    std::unique_ptr<ProfileOAuth2TokenService> token_service,
    std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service,
    std::unique_ptr<PrimaryAccountManager> primary_account_manager,
    std::unique_ptr<AccountFetcherService> account_fetcher_service,
    std::unique_ptr<identity::PrimaryAccountMutator> primary_account_mutator,
    std::unique_ptr<identity::AccountsMutator> accounts_mutator,
    std::unique_ptr<identity::AccountsCookieMutator> accounts_cookie_mutator,
    std::unique_ptr<identity::DiagnosticsProvider> diagnostics_provider)
    : identity::IdentityManager(std::move(account_tracker_service),
                                std::move(token_service),
                                std::move(gaia_cookie_manager_service),
                                std::move(primary_account_manager),
                                std::move(account_fetcher_service),
                                std::move(primary_account_mutator),
                                std::move(accounts_mutator),
                                std::move(accounts_cookie_mutator),
                                std::move(diagnostics_provider)) {}
