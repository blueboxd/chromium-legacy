// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_manager.h"

#include "base/time/time.h"

OAuth2AccessTokenManager::OAuth2AccessTokenManager() = default;

OAuth2AccessTokenManager::~OAuth2AccessTokenManager() = default;

void OAuth2AccessTokenManager::RegisterTokenResponse(
    const std::string& client_id,
    const CoreAccountId& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  token_cache_[OAuth2TokenService::RequestParameters(client_id, account_id,
                                                     scopes)] = token_response;
}

const OAuth2AccessTokenConsumer::TokenResponse*
OAuth2AccessTokenManager::GetCachedTokenResponse(
    const OAuth2TokenService::RequestParameters& request_parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OAuth2TokenService::TokenCache::iterator token_iterator =
      token_cache_.find(request_parameters);
  if (token_iterator == token_cache_.end())
    return nullptr;
  if (token_iterator->second.expiration_time <= base::Time::Now()) {
    token_cache_.erase(token_iterator);
    return nullptr;
  }
  return &token_iterator->second;
}
