// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_H_

#include "base/sequence_checker.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
// TODO(https://crbug.com/967598): Remove this once OAuth2AccessTokenManager's
// separated from OAuth2TokenService.
#include "google_apis/gaia/oauth2_token_service.h"

// Class that manages requests for OAuth2 access tokens.
class OAuth2AccessTokenManager {
 public:
  OAuth2AccessTokenManager();
  virtual ~OAuth2AccessTokenManager();

  // Add a new entry to the cache.
  void RegisterTokenResponse(
      const std::string& client_id,
      const CoreAccountId& account_id,
      const OAuth2TokenService::ScopeSet& scopes,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  // Returns a currently valid OAuth2 access token for the given set of scopes,
  // or NULL if none have been cached. Note the user of this method should
  // ensure no entry with the same |client_scopes| is added before the usage of
  // the returned entry is done.
  const OAuth2AccessTokenConsumer::TokenResponse* GetCachedTokenResponse(
      const OAuth2TokenService::RequestParameters& client_scopes);

 private:
  // TODO(https://crbug.com/967598): Remove this once |token_cache_| management
  // is moved to OAuth2AccessTokenManager.
  friend class OAuth2TokenService;

  OAuth2TokenService::TokenCache& token_cache() { return token_cache_; }

  // The cache of currently valid tokens.
  OAuth2TokenService::TokenCache token_cache_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(OAuth2AccessTokenManager);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_H_
