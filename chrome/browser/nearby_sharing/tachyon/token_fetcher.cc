// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/tachyon/token_fetcher.h"

#include "google_apis/gaia/gaia_constants.h"

namespace {
// The oauth token consumer name.
const char kOAuthConsumerName[] = "nearby_sharing";
}  // namespace

TokenFetcher::TokenFetcher(signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {}

TokenFetcher::~TokenFetcher() = default;

void TokenFetcher::GetAccessToken(
    base::OnceCallback<void(const std::string& token)> callback) {
  // Using Mode::kWaitUntilRefreshTokenAvailable waits for the account to have a
  // refresh token which can take forever if the user is not signed in (and
  // doesnt sign in). Since nearby sharing is only available for already signed
  // in users, we are using this mode to ensure that best effort in trying to
  // get a token.

  token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      identity_manager_->GetPrimaryAccountId(
          signin::ConsentLevel::kNotRequired),
      kOAuthConsumerName, {GaiaConstants::kTachyonOAuthScope},
      base::BindOnce(&TokenFetcher::OnOAuthTokenFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      signin::AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);
}

void TokenFetcher::OnOAuthTokenFetched(
    base::OnceCallback<void(const std::string& token)> callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  // TODO(himanshujaju) - The token returned is empty for error, should we do
  // something special?
  std::move(callback).Run(access_token_info.token);
  token_fetcher_.reset();
}
