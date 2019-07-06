// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_token_service.h"

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

OAuth2TokenService::OAuth2TokenService(
    std::unique_ptr<OAuth2TokenServiceDelegate> delegate)
    : delegate_(std::move(delegate)), all_credentials_loaded_(false) {
  DCHECK(delegate_);
  AddObserver(this);
  token_manager_ = std::make_unique<OAuth2AccessTokenManager>(
      this /* OAuth2AccessTokenManager::Delegate* */);
}

OAuth2TokenService::~OAuth2TokenService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserver(this);
}

OAuth2TokenServiceDelegate* OAuth2TokenService::GetDelegate() {
  return delegate_.get();
}

const OAuth2TokenServiceDelegate* OAuth2TokenService::GetDelegate() const {
  return delegate_.get();
}

OAuth2AccessTokenManager* OAuth2TokenService::GetAccessTokenManager() {
  return token_manager_.get();
}

const base::ObserverList<OAuth2AccessTokenManager::DiagnosticsObserver,
                         true>::Unchecked&
OAuth2TokenService::GetAccessTokenDiagnosticsObservers() {
  return token_manager_->diagnostics_observer_list_;
}

int OAuth2TokenService::GetTokenCacheCount() {
  return token_manager_->token_cache().size();
}

std::unique_ptr<OAuth2AccessTokenFetcher>
OAuth2TokenService::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  return delegate_->CreateAccessTokenFetcher(account_id, url_loader_factory,
                                             consumer);
}

bool OAuth2TokenService::FixRequestErrorIfPossible() {
  return delegate_->FixRequestErrorIfPossible();
}

scoped_refptr<network::SharedURLLoaderFactory>
OAuth2TokenService::GetURLLoaderFactory() const {
  return delegate_->GetURLLoaderFactory();
}

void OAuth2TokenService::OnAccessTokenInvalidated(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    const std::string& access_token) {
  delegate_->OnAccessTokenInvalidated(account_id, client_id, scopes,
                                      access_token);
}

void OAuth2TokenService::OnAccessTokenFetched(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  // Update the auth error state so auth errors are appropriately communicated
  // to the user.
  delegate_->UpdateAuthError(account_id, error);
}

bool OAuth2TokenService::HasRefreshToken(
    const CoreAccountId& account_id) const {
  return RefreshTokenIsAvailable(account_id);
}

void OAuth2TokenService::AddObserver(OAuth2TokenServiceObserver* observer) {
  delegate_->AddObserver(observer);
}

void OAuth2TokenService::RemoveObserver(OAuth2TokenServiceObserver* observer) {
  delegate_->RemoveObserver(observer);
}

void OAuth2TokenService::AddAccessTokenDiagnosticsObserver(
    OAuth2AccessTokenManager::DiagnosticsObserver* observer) {
  token_manager_->AddDiagnosticsObserver(observer);
}

void OAuth2TokenService::RemoveAccessTokenDiagnosticsObserver(
    OAuth2AccessTokenManager::DiagnosticsObserver* observer) {
  token_manager_->RemoveDiagnosticsObserver(observer);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
OAuth2TokenService::StartRequestForMultilogin(
    const CoreAccountId& account_id,
    OAuth2AccessTokenManager::Consumer* consumer) {
  const std::string refresh_token =
      delegate_->GetTokenForMultilogin(account_id);
  if (refresh_token.empty()) {
    // If we can't get refresh token from the delegate, start request for access
    // token.
    OAuth2AccessTokenManager::ScopeSet scopes;
    scopes.insert(GaiaConstants::kOAuth1LoginScope);
    return StartRequest(account_id, scopes, consumer);
  }
  std::unique_ptr<OAuth2AccessTokenManager::RequestImpl> request(
      new OAuth2AccessTokenManager::RequestImpl(account_id, consumer));
  // Create token response from token. Expiration time and id token do not
  // matter and should not be accessed.
  OAuth2AccessTokenConsumer::TokenResponse token_response(
      refresh_token, base::Time(), std::string());
  // If we can get refresh token from the delegate, inform cosumer right away.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OAuth2AccessTokenManager::RequestImpl::InformConsumer,
                     request.get()->AsWeakPtr(),
                     GoogleServiceAuthError(GoogleServiceAuthError::NONE),
                     token_response));
  return std::move(request);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
OAuth2TokenService::StartRequest(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    OAuth2AccessTokenManager::Consumer* consumer) {
  return token_manager_->StartRequest(account_id, scopes, consumer);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
OAuth2TokenService::StartRequestForClient(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    OAuth2AccessTokenManager::Consumer* consumer) {
  return token_manager_->StartRequestForClient(account_id, client_id,
                                               client_secret, scopes, consumer);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
OAuth2TokenService::StartRequestWithContext(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    OAuth2AccessTokenManager::Consumer* consumer) {
  return token_manager_->StartRequestWithContext(account_id, url_loader_factory,
                                                 scopes, consumer);
}

bool OAuth2TokenService::AreAllCredentialsLoaded() const {
  return all_credentials_loaded_;
}

std::vector<CoreAccountId> OAuth2TokenService::GetAccounts() const {
  if (!AreAllCredentialsLoaded())
    return std::vector<CoreAccountId>();

  return delegate_->GetAccounts();
}

bool OAuth2TokenService::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  return delegate_->RefreshTokenIsAvailable(account_id);
}

bool OAuth2TokenService::RefreshTokenHasError(
    const CoreAccountId& account_id) const {
  return GetAuthError(account_id) != GoogleServiceAuthError::AuthErrorNone();
}

GoogleServiceAuthError OAuth2TokenService::GetAuthError(
    const CoreAccountId& account_id) const {
  GoogleServiceAuthError error = delegate_->GetAuthError(account_id);
  DCHECK(!error.IsTransientError());
  return error;
}

void OAuth2TokenService::InvalidateAccessToken(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  token_manager_->InvalidateAccessToken(account_id, scopes, access_token);
}

void OAuth2TokenService::InvalidateTokenForMultilogin(
    const CoreAccountId& failed_account,
    const std::string& token) {
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  // Remove from cache. This will have no effect on desktop since token is a
  // refresh token and is not in cache.
  InvalidateAccessToken(failed_account, scopes, token);
  // For desktop refresh tokens can be invalidated directly in delegate. This
  // will have no effect on mobile.
  delegate_->InvalidateTokenForMultilogin(failed_account);
}

void OAuth2TokenService::OnRefreshTokensLoaded() {
  all_credentials_loaded_ = true;
}

void OAuth2TokenService::RegisterTokenResponse(
    const std::string& client_id,
    const CoreAccountId& account_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_manager_->RegisterTokenResponse(client_id, account_id, scopes,
                                        token_response);
}

void OAuth2TokenService::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_manager_->ClearCache();
}

void OAuth2TokenService::ClearCacheForAccount(const CoreAccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_manager_->ClearCacheForAccount(account_id);
}

void OAuth2TokenService::CancelAllRequests() {
  token_manager_->CancelAllRequests();
}

void OAuth2TokenService::CancelRequestsForAccount(
    const CoreAccountId& account_id) {
  token_manager_->CancelRequestsForAccount(account_id);
}

void OAuth2TokenService::set_max_authorization_token_fetch_retries_for_testing(
    int max_retries) {
  token_manager_->set_max_authorization_token_fetch_retries_for_testing(
      max_retries);
}

size_t OAuth2TokenService::GetNumPendingRequestsForTesting(
    const std::string& client_id,
    const CoreAccountId& account_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes) const {
  return token_manager_->GetNumPendingRequestsForTesting(client_id, account_id,
                                                         scopes);
}

void OAuth2TokenService::OverrideAccessTokenManagerForTesting(
    std::unique_ptr<OAuth2AccessTokenManager> token_manager) {
  token_manager_ = std::move(token_manager);
}
