// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_INFO_FETCHER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_INFO_FETCHER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace network {
class SharedURLLoaderFactory;
}

class AccountFetcherService;

// An account information fetcher that gets an OAuth token of appropriate
// scope and uses it to fetch account infromation. This does not handle
// refreshing the information and is meant to be used in a one shot fashion.
class AccountInfoFetcher : public OAuth2TokenService::Consumer,
                           public gaia::GaiaOAuthClient::Delegate {
 public:
  AccountInfoFetcher(
      OAuth2TokenService* token_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AccountFetcherService* service,
      const CoreAccountId& account_id);
  ~AccountInfoFetcher() override;

  const CoreAccountId& account_id() { return account_id_; }

  // Start fetching the account information.
  void Start();

  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetUserInfoResponse(
      std::unique_ptr<base::DictionaryValue> user_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  OAuth2TokenService* token_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  AccountFetcherService* service_;
  const CoreAccountId account_id_;

  std::unique_ptr<OAuth2TokenService::Request> login_token_request_;
  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;

  DISALLOW_COPY_AND_ASSIGN(AccountInfoFetcher);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_INFO_FETCHER_H_
