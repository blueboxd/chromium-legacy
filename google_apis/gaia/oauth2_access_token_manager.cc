// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_manager.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

OAuth2AccessTokenManager::OAuth2AccessTokenManager(
    OAuth2TokenService* token_service,
    OAuth2TokenServiceDelegate* delegate)
    : token_service_(token_service), delegate_(delegate) {
  DCHECK(token_service_);
  DCHECK(delegate_);
}

OAuth2AccessTokenManager::~OAuth2AccessTokenManager() = default;

void OAuth2AccessTokenManager::AddDiagnosticsObserver(
    AccessTokenDiagnosticsObserver* observer) {
  diagnostics_observer_list_.AddObserver(observer);
}

void OAuth2AccessTokenManager::RemoveDiagnosticsObserver(
    AccessTokenDiagnosticsObserver* observer) {
  diagnostics_observer_list_.RemoveObserver(observer);
}

std::unique_ptr<OAuth2TokenService::Request>
OAuth2AccessTokenManager::StartRequest(
    const CoreAccountId& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  return StartRequestForClientWithContext(
      account_id, delegate_->GetURLLoaderFactory(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(), scopes, consumer);
}

std::unique_ptr<OAuth2TokenService::Request>
OAuth2AccessTokenManager::StartRequestForClient(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  return StartRequestForClientWithContext(
      account_id, delegate_->GetURLLoaderFactory(), client_id, client_secret,
      scopes, consumer);
}

std::unique_ptr<OAuth2TokenService::Request>
OAuth2AccessTokenManager::StartRequestWithContext(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  return StartRequestForClientWithContext(
      account_id, url_loader_factory,
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(), scopes, consumer);
}

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

void OAuth2AccessTokenManager::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& entry : token_cache_) {
    for (auto& observer : diagnostics_observer_list_)
      observer.OnAccessTokenRemoved(entry.first.account_id, entry.first.scopes);
  }

  token_cache_.clear();
}

void OAuth2AccessTokenManager::ClearCacheForAccount(
    const CoreAccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (OAuth2TokenService::TokenCache::iterator iter = token_cache_.begin();
       iter != token_cache_.end();
       /* iter incremented in body */) {
    if (iter->first.account_id == account_id) {
      for (auto& observer : diagnostics_observer_list_)
        observer.OnAccessTokenRemoved(account_id, iter->first.scopes);
      token_cache_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

std::unique_ptr<OAuth2TokenService::Request>
OAuth2AccessTokenManager::StartRequestForClientWithContext(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<OAuth2TokenService::RequestImpl> request(
      new OAuth2TokenService::RequestImpl(account_id, consumer));
  for (auto& observer : diagnostics_observer_list_)
    observer.OnAccessTokenRequested(account_id, consumer->id(), scopes);

  if (!delegate_->RefreshTokenIsAvailable(account_id)) {
    GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);

    for (auto& observer : diagnostics_observer_list_) {
      observer.OnFetchAccessTokenComplete(account_id, consumer->id(), scopes,
                                          error, base::Time());
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&OAuth2TokenService::RequestImpl::InformConsumer,
                       request->AsWeakPtr(), error,
                       OAuth2AccessTokenConsumer::TokenResponse()));
    return std::move(request);
  }

  OAuth2TokenService::RequestParameters request_parameters(client_id,
                                                           account_id, scopes);
  const OAuth2AccessTokenConsumer::TokenResponse* token_response =
      GetCachedTokenResponse(request_parameters);
  if (token_response && token_response->access_token.length()) {
    InformConsumerWithCachedTokenResponse(token_response, request.get(),
                                          request_parameters);
  } else {
    token_service_->FetchOAuth2Token(request.get(), account_id,
                                     url_loader_factory, client_id,
                                     client_secret, scopes);
  }
  return std::move(request);
}

void OAuth2AccessTokenManager::InformConsumerWithCachedTokenResponse(
    const OAuth2AccessTokenConsumer::TokenResponse* cache_token_response,
    OAuth2TokenService::RequestImpl* request,
    const OAuth2TokenService::RequestParameters& request_parameters) {
  DCHECK(cache_token_response && cache_token_response->access_token.length());
  for (auto& observer : diagnostics_observer_list_) {
    observer.OnFetchAccessTokenComplete(
        request_parameters.account_id, request->GetConsumerId(),
        request_parameters.scopes, GoogleServiceAuthError::AuthErrorNone(),
        cache_token_response->expiration_time);
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OAuth2TokenService::RequestImpl::InformConsumer,
                     request->AsWeakPtr(),
                     GoogleServiceAuthError(GoogleServiceAuthError::NONE),
                     *cache_token_response));
}

bool OAuth2AccessTokenManager::RemoveCachedTokenResponse(
    const OAuth2TokenService::RequestParameters& request_parameters,
    const std::string& token_to_remove) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OAuth2TokenService::OAuth2TokenService::TokenCache::iterator token_iterator =
      token_cache_.find(request_parameters);
  if (token_iterator != token_cache_.end() &&
      token_iterator->second.access_token == token_to_remove) {
    for (auto& observer : diagnostics_observer_list_) {
      observer.OnAccessTokenRemoved(request_parameters.account_id,
                                    request_parameters.scopes);
    }
    token_cache_.erase(token_iterator);
    return true;
  }
  return false;
}
