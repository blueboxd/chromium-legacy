// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_oauth2_token_service.h"

#include <memory>

FakeOAuth2TokenService::FakeOAuth2TokenService()
    : OAuth2TokenService(std::make_unique<FakeOAuth2TokenServiceDelegate>()) {
  OverrideAccessTokenManagerForTesting(
      std::make_unique<FakeOAuth2AccessTokenManager>(
          this /* OAuth2AccessTokenManager::Delegate* */));
}

FakeOAuth2TokenService::~FakeOAuth2TokenService() {
}

void FakeOAuth2TokenService::AddAccount(const CoreAccountId& account_id) {
  GetDelegate()->UpdateCredentials(account_id, "fake_refresh_token");
}

void FakeOAuth2TokenService::RemoveAccount(const CoreAccountId& account_id) {
  GetDelegate()->RevokeCredentials(account_id);
}

void FakeOAuth2TokenService::IssueAllTokensForAccount(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  GetFakeAccessTokenManager()->IssueAllTokensForAccount(account_id,
                                                        token_response);
}

void FakeOAuth2TokenService::IssueErrorForAllPendingRequestsForAccount(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& auth_error) {
  GetFakeAccessTokenManager()->IssueErrorForAllPendingRequestsForAccount(
      account_id, auth_error);
}

FakeOAuth2TokenServiceDelegate*
FakeOAuth2TokenService::GetFakeOAuth2TokenServiceDelegate() {
  return static_cast<FakeOAuth2TokenServiceDelegate*>(GetDelegate());
}

FakeOAuth2AccessTokenManager*
FakeOAuth2TokenService::GetFakeAccessTokenManager() {
  return static_cast<FakeOAuth2AccessTokenManager*>(GetAccessTokenManager());
}
