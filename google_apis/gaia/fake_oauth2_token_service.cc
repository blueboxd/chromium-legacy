// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_oauth2_token_service.h"

#include <memory>

FakeOAuth2TokenService::PendingRequest::PendingRequest() {
}

FakeOAuth2TokenService::PendingRequest::PendingRequest(
    const PendingRequest& other) = default;

FakeOAuth2TokenService::PendingRequest::~PendingRequest() {
}

FakeOAuth2TokenService::FakeOAuth2TokenService()
    : OAuth2TokenService(std::make_unique<FakeOAuth2TokenServiceDelegate>()) {}

FakeOAuth2TokenService::~FakeOAuth2TokenService() {
}

void FakeOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  PendingRequest pending_request;
  pending_request.account_id = account_id;
  pending_request.client_id = client_id;
  pending_request.client_secret = client_secret;
  pending_request.scopes = scopes;
  pending_request.request = request->AsWeakPtr();
  pending_requests_.push_back(pending_request);
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
  // Walk the requests and notify the callbacks.
  // Using a copy of pending requests to make sure a new token request triggered
  // from the handling code does not invalidate the iterator.
  std::vector<PendingRequest> pending_requests_copy = pending_requests_;
  for (std::vector<PendingRequest>::iterator it = pending_requests_copy.begin();
       it != pending_requests_copy.end();
       ++it) {
    if (it->request && (account_id == it->account_id)) {
      it->request->InformConsumer(GoogleServiceAuthError::AuthErrorNone(),
                                  token_response);
    }
  }
}

void FakeOAuth2TokenService::IssueErrorForAllPendingRequestsForAccount(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& auth_error) {
  // Walk the requests and notify the callbacks.
  // Using a copy of pending requests to make sure retrying a request in
  // response to the error does not invalidate the iterator.
  std::vector<PendingRequest> pending_requests_copy = pending_requests_;
  for (std::vector<PendingRequest>::iterator it = pending_requests_copy.begin();
       it != pending_requests_copy.end();
       ++it) {
    if (it->request && (account_id == it->account_id)) {
      it->request->InformConsumer(auth_error,
                                  OAuth2AccessTokenConsumer::TokenResponse());
    }
  }
}

FakeOAuth2TokenServiceDelegate*
FakeOAuth2TokenService::GetFakeOAuth2TokenServiceDelegate() {
  return static_cast<FakeOAuth2TokenServiceDelegate*>(GetDelegate());
}
