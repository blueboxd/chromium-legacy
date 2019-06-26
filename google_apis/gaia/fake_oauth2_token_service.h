// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_FAKE_OAUTH2_TOKEN_SERVICE_H_
#define GOOGLE_APIS_GAIA_FAKE_OAUTH2_TOKEN_SERVICE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace network {
class SharedURLLoaderFactory;
}

// Do-nothing implementation of OAuth2TokenService.
class FakeOAuth2TokenService : public OAuth2TokenService {
 public:
  FakeOAuth2TokenService();
  ~FakeOAuth2TokenService() override;

  void AddAccount(const CoreAccountId& account_id);
  void RemoveAccount(const CoreAccountId& account_id);

  // Helper routines to issue tokens for pending requests or complete them with
  // error.
  void IssueAllTokensForAccount(
      const CoreAccountId& account_id,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);
  void IssueErrorForAllPendingRequestsForAccount(
      const CoreAccountId& account_id,
      const GoogleServiceAuthError& auth_error);

  void InvalidateTokenForMultilogin(const CoreAccountId& account_id,
                                    const std::string& token) override {}

  FakeOAuth2TokenServiceDelegate* GetFakeOAuth2TokenServiceDelegate();

 protected:
  // OAuth2TokenService overrides.
  void FetchOAuth2Token(
      OAuth2AccessTokenManager::RequestImpl* request,
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2AccessTokenManager::ScopeSet& scopes) override;

  void InvalidateAccessTokenImpl(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const std::string& access_token) override {}

 private:
  struct PendingRequest {
    PendingRequest();
    PendingRequest(const PendingRequest& other);
    ~PendingRequest();

    CoreAccountId account_id;
    std::string client_id;
    std::string client_secret;
    OAuth2AccessTokenManager::ScopeSet scopes;
    base::WeakPtr<OAuth2AccessTokenManager::RequestImpl> request;
  };

  std::vector<PendingRequest> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(FakeOAuth2TokenService);
};

#endif  // GOOGLE_APIS_GAIA_FAKE_OAUTH2_TOKEN_SERVICE_H_
