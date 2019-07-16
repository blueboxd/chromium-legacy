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
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
  token_manager_ = std::make_unique<OAuth2AccessTokenManager>(
      this /* OAuth2AccessTokenManager::Delegate* */);
}

OAuth2TokenService::~OAuth2TokenService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  return delegate_->RefreshTokenIsAvailable(account_id);
}
