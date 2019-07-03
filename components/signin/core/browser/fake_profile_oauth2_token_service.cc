// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"

#include <memory>

#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

FakeProfileOAuth2TokenService::FakeProfileOAuth2TokenService(
    PrefService* user_prefs)
    : FakeProfileOAuth2TokenService(
          user_prefs,
          std::make_unique<FakeOAuth2TokenServiceDelegate>()) {}

FakeProfileOAuth2TokenService::FakeProfileOAuth2TokenService(
    PrefService* user_prefs,
    std::unique_ptr<OAuth2TokenServiceDelegate> delegate)
    : ProfileOAuth2TokenService(user_prefs, std::move(delegate)) {
  OverrideAccessTokenManagerForTesting(
      std::make_unique<FakeOAuth2AccessTokenManager>(
          this /* OAuth2TokenService* */,
          this /* OAuth2AccessTokenManager::Delegate* */));
}

FakeProfileOAuth2TokenService::~FakeProfileOAuth2TokenService() {}

void FakeProfileOAuth2TokenService::IssueAllTokensForAccount(
    const std::string& account_id,
    const std::string& access_token,
    const base::Time& expiration) {
  GetFakeAccessTokenManager()->IssueAllTokensForAccount(
      account_id, access_token, expiration);
}

void FakeProfileOAuth2TokenService::IssueAllTokensForAccount(
    const std::string& account_id,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  GetFakeAccessTokenManager()->IssueAllTokensForAccount(account_id,
                                                        token_response);
}

void FakeProfileOAuth2TokenService::IssueErrorForAllPendingRequestsForAccount(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  GetFakeAccessTokenManager()->IssueErrorForAllPendingRequestsForAccount(
      account_id, error);
}

void FakeProfileOAuth2TokenService::IssueTokenForScope(
    const OAuth2AccessTokenManager::ScopeSet& scope,
    const std::string& access_token,
    const base::Time& expiration) {
  GetFakeAccessTokenManager()->IssueTokenForScope(scope, access_token,
                                                  expiration);
}

void FakeProfileOAuth2TokenService::IssueTokenForScope(
    const OAuth2AccessTokenManager::ScopeSet& scope,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  GetFakeAccessTokenManager()->IssueTokenForScope(scope, token_response);
}

void FakeProfileOAuth2TokenService::IssueErrorForScope(
    const OAuth2AccessTokenManager::ScopeSet& scope,
    const GoogleServiceAuthError& error) {
  GetFakeAccessTokenManager()->IssueErrorForScope(scope, error);
}

void FakeProfileOAuth2TokenService::IssueErrorForAllPendingRequests(
    const GoogleServiceAuthError& error) {
  GetFakeAccessTokenManager()->IssueErrorForAllPendingRequests(error);
}

void FakeProfileOAuth2TokenService::
    set_auto_post_fetch_response_on_message_loop(bool auto_post_response) {
  GetFakeAccessTokenManager()->set_auto_post_fetch_response_on_message_loop(
      auto_post_response);
}

void FakeProfileOAuth2TokenService::IssueTokenForAllPendingRequests(
    const std::string& access_token,
    const base::Time& expiration) {
  GetFakeAccessTokenManager()->IssueTokenForAllPendingRequests(access_token,
                                                               expiration);
}

void FakeProfileOAuth2TokenService::IssueTokenForAllPendingRequests(
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  GetFakeAccessTokenManager()->IssueTokenForAllPendingRequests(token_response);
}

std::vector<FakeOAuth2AccessTokenManager::PendingRequest>
FakeProfileOAuth2TokenService::GetPendingRequests() {
  return GetFakeAccessTokenManager()->GetPendingRequests();
}

void FakeProfileOAuth2TokenService::CancelAllRequests() {
  GetFakeAccessTokenManager()->CancelAllRequests();
}

void FakeProfileOAuth2TokenService::CancelRequestsForAccount(
    const CoreAccountId& account_id) {
  GetFakeAccessTokenManager()->CancelRequestsForAccount(account_id);
}

void FakeProfileOAuth2TokenService::FetchOAuth2Token(
    OAuth2AccessTokenManager::RequestImpl* request,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2AccessTokenManager::ScopeSet& scopes) {
  GetFakeAccessTokenManager()->FetchOAuth2Token(request, account_id,
                                                url_loader_factory, client_id,
                                                client_secret, scopes);
}

void FakeProfileOAuth2TokenService::InvalidateAccessTokenImpl(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  GetFakeAccessTokenManager()->InvalidateAccessTokenImpl(account_id, client_id,
                                                         scopes, access_token);
}

FakeOAuth2AccessTokenManager*
FakeProfileOAuth2TokenService::GetFakeAccessTokenManager() {
  return static_cast<FakeOAuth2AccessTokenManager*>(GetAccessTokenManager());
}
