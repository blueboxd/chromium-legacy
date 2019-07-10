// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

struct DeviceOAuth2TokenService::PendingRequest {
  PendingRequest(
      const base::WeakPtr<OAuth2AccessTokenManager::RequestImpl>& request,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2AccessTokenManager::ScopeSet& scopes)
      : request(request),
        client_id(client_id),
        client_secret(client_secret),
        scopes(scopes) {}

  const base::WeakPtr<OAuth2AccessTokenManager::RequestImpl> request;
  const std::string client_id;
  const std::string client_secret;
  const OAuth2AccessTokenManager::ScopeSet scopes;
};

void DeviceOAuth2TokenService::OnValidationCompleted(
    GoogleServiceAuthError::State error) {
  if (error == GoogleServiceAuthError::NONE)
    FlushPendingRequests(true, GoogleServiceAuthError::NONE);
  else
    FlushPendingRequests(false, error);
}

DeviceOAuth2TokenService::DeviceOAuth2TokenService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_state) {
  delegate_ = std::make_unique<DeviceOAuth2TokenServiceDelegate>(
      url_loader_factory, local_state, this);
  token_manager_ = std::make_unique<OAuth2AccessTokenManager>(
      this /* OAuth2AccessTokenManager::Delegate* */);
  delegate_->InitializeWithValidationStatusDelegate(this);
}

DeviceOAuth2TokenService::~DeviceOAuth2TokenService() {
  delegate_->ClearValidationStatusDelegate();
  FlushPendingRequests(false, GoogleServiceAuthError::REQUEST_CANCELED);
}

// static
void DeviceOAuth2TokenService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceRobotAnyApiRefreshToken,
                               std::string());
}

void DeviceOAuth2TokenService::SetAndSaveRefreshToken(
    const std::string& refresh_token,
    const StatusCallback& result_callback) {
  delegate_->SetAndSaveRefreshToken(refresh_token, result_callback);
}

std::string DeviceOAuth2TokenService::GetRobotAccountId() const {
  return delegate_->GetRobotAccountId();
}

void DeviceOAuth2TokenService::set_robot_account_id_for_testing(
    const CoreAccountId& account_id) {
  delegate_->set_robot_account_id_for_testing(account_id);
}

void DeviceOAuth2TokenService::SetRefreshTokenAvailableCallback(
    RefreshTokenAvailableCallback callback) {
  on_refresh_token_available_callback_ = std::move(callback);
}

void DeviceOAuth2TokenService::SetRefreshTokenRevokedCallback(
    RefreshTokenRevokedCallback callback) {
  on_refresh_token_revoked_callback_ = std::move(callback);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
DeviceOAuth2TokenService::StartAccessTokenRequest(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    OAuth2AccessTokenManager::Consumer* consumer) {
  return token_manager_->StartRequest(account_id, scopes, consumer);
}

void DeviceOAuth2TokenService::InvalidateAccessToken(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  token_manager_->InvalidateAccessToken(account_id, scopes, access_token);
}

bool DeviceOAuth2TokenService::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  return delegate_->RefreshTokenIsAvailable(account_id);
}

OAuth2AccessTokenManager* DeviceOAuth2TokenService::GetAccessTokenManager() {
  return token_manager_.get();
}

std::unique_ptr<OAuth2AccessTokenFetcher>
DeviceOAuth2TokenService::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  return delegate_->CreateAccessTokenFetcher(account_id, url_loader_factory,
                                             consumer);
}

bool DeviceOAuth2TokenService::HasRefreshToken(
    const CoreAccountId& account_id) const {
  return RefreshTokenIsAvailable(account_id);
}

scoped_refptr<network::SharedURLLoaderFactory>
DeviceOAuth2TokenService::GetURLLoaderFactory() const {
  return delegate_->GetURLLoaderFactory();
}

void DeviceOAuth2TokenService::FireRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  if (on_refresh_token_available_callback_)
    on_refresh_token_available_callback_.Run(account_id);
}

void DeviceOAuth2TokenService::FireRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  if (on_refresh_token_revoked_callback_)
    on_refresh_token_revoked_callback_.Run(account_id);
}

bool DeviceOAuth2TokenService::HandleAccessTokenFetch(
    OAuth2AccessTokenManager::RequestImpl* request,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2AccessTokenManager::ScopeSet& scopes) {
  switch (delegate_->state_) {
    case DeviceOAuth2TokenServiceDelegate::STATE_VALIDATION_PENDING:
      // If this is the first request for a token, start validation.
      delegate_->StartValidation();
      FALLTHROUGH;
    case DeviceOAuth2TokenServiceDelegate::STATE_LOADING:
    case DeviceOAuth2TokenServiceDelegate::STATE_VALIDATION_STARTED:
      // Add a pending request that will be satisfied once validation completes.
      pending_requests_.push_back(new PendingRequest(
          request->AsWeakPtr(), client_id, client_secret, scopes));
      delegate_->RequestValidation();
      return true;
    case DeviceOAuth2TokenServiceDelegate::STATE_NO_TOKEN:
      FailRequest(request, GoogleServiceAuthError::USER_NOT_SIGNED_UP);
      return true;
    case DeviceOAuth2TokenServiceDelegate::STATE_TOKEN_INVALID:
      FailRequest(request, GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
      return true;
    case DeviceOAuth2TokenServiceDelegate::STATE_TOKEN_VALID:
      // Let OAuth2AccessTokenManager handle the request.
      return false;
  }

  NOTREACHED() << "Unexpected state " << delegate_->state_;
  return false;
}

void DeviceOAuth2TokenService::FlushPendingRequests(
    bool token_is_valid,
    GoogleServiceAuthError::State error) {
  std::vector<PendingRequest*> requests;
  requests.swap(pending_requests_);
  for (std::vector<PendingRequest*>::iterator request(requests.begin());
       request != requests.end();
       ++request) {
    std::unique_ptr<PendingRequest> scoped_request(*request);
    if (!scoped_request->request)
      continue;

    if (token_is_valid) {
      token_manager_->FetchOAuth2Token(
          scoped_request->request.get(),
          scoped_request->request->GetAccountId(),
          delegate_->GetURLLoaderFactory(), scoped_request->client_id,
          scoped_request->client_secret, scoped_request->scopes);
    } else {
      FailRequest(scoped_request->request.get(), error);
    }
  }
}

void DeviceOAuth2TokenService::FailRequest(
    OAuth2AccessTokenManager::RequestImpl* request,
    GoogleServiceAuthError::State error) {
  GoogleServiceAuthError auth_error =
      (error == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)
          ? GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_SERVER)
          : GoogleServiceAuthError(error);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OAuth2AccessTokenManager::RequestImpl::InformConsumer,
                     request->AsWeakPtr(), auth_error,
                     OAuth2AccessTokenConsumer::TokenResponse()));
}
}  // namespace chromeos
