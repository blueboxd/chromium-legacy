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
  // TODO(https://crbug.com/967598): Remove |token_service| parameter once
  // OAuth2AccessTokenManager fully manages access tokens independently of
  // OAuth2TokenService.
  explicit OAuth2AccessTokenManager(OAuth2TokenService* token_service);
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

  // Clears the internal token cache.
  void ClearCache();

  // Clears all of the tokens belonging to |account_id| from the internal token
  // cache. It does not matter what other parameters, like |client_id| were
  // used to request the tokens.
  void ClearCacheForAccount(const CoreAccountId& account_id);

 private:
  // TODO(https://crbug.com/967598): Remove this once |token_cache_| management
  // is moved to OAuth2AccessTokenManager.
  friend class OAuth2TokenService;

  OAuth2TokenService::TokenCache& token_cache() { return token_cache_; }

  // Removes an access token for the given set of scopes from the cache.
  // Returns true if the entry was removed, otherwise false.
  bool RemoveCachedTokenResponse(
      const OAuth2TokenService::RequestParameters& client_scopes,
      const std::string& token_to_remove);

  // The cache of currently valid tokens.
  OAuth2TokenService::TokenCache token_cache_;
  // TODO(https://crbug.com/967598): Remove this once OAuth2AccessTokenManager
  // fully manages access tokens independently of OAuth2TokenService.
  OAuth2TokenService* token_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(OAuth2AccessTokenManager);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_H_
