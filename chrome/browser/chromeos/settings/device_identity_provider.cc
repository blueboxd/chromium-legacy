// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_identity_provider.h"

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "components/invalidation/public/active_account_access_token_fetcher_impl.h"

namespace chromeos {

DeviceIdentityProvider::DeviceIdentityProvider(
    chromeos::DeviceOAuth2TokenService* token_service)
    : token_service_(token_service) {
  // TODO(blundell): Can |token_service_| ever actually be non-null?
  if (token_service_)
    token_service_->AddObserver(this);
}

DeviceIdentityProvider::~DeviceIdentityProvider() {
  // TODO(blundell): Can |token_service_| ever actually be non-null?
  if (token_service_)
    token_service_->RemoveObserver(this);
}

CoreAccountId DeviceIdentityProvider::GetActiveAccountId() {
  return token_service_->GetRobotAccountId();
}

void DeviceIdentityProvider::SetActiveAccountId(
    const CoreAccountId& account_id) {
  // On ChromeOs, the account shouldn't change during runtime, so no need to
  // alert observers here.
  if (!account_id.empty()) {
    auto robot_account_id = token_service_->GetRobotAccountId();
    // When |account_id| and |robot_account_id| mismatch, it means that sync is
    // using a different account than the one that's registered for
    // invalidations. Given that we're in Kiosk mode, sync shouldn't be running
    // anyways. Therefore, this shouldn't be a problem in practice.
    // TODO(crbug.com/919788): Change the sync code to only call this method
    // when sync is actually running.
    LOG_IF(WARNING, account_id != robot_account_id) << "Account ids mismatch.";
  }
  return;
}

bool DeviceIdentityProvider::IsActiveAccountWithRefreshToken() {
  if (GetActiveAccountId().empty() || !token_service_ ||
      !token_service_->RefreshTokenIsAvailable(GetActiveAccountId()))
    return false;

  return true;
}

std::unique_ptr<invalidation::ActiveAccountAccessTokenFetcher>
DeviceIdentityProvider::FetchAccessToken(
    const std::string& oauth_consumer_name,
    const OAuth2TokenService::ScopeSet& scopes,
    invalidation::ActiveAccountAccessTokenCallback callback) {
  return std::make_unique<invalidation::ActiveAccountAccessTokenFetcherImpl>(
      GetActiveAccountId(), oauth_consumer_name, token_service_, scopes,
      std::move(callback));
}

void DeviceIdentityProvider::InvalidateAccessToken(
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token) {
  token_service_->InvalidateAccessToken(GetActiveAccountId(), scopes,
                                        access_token);
}

void DeviceIdentityProvider::OnRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  ProcessRefreshTokenUpdateForAccount(account_id);
}

void DeviceIdentityProvider::OnRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  ProcessRefreshTokenRemovalForAccount(account_id);
}

}  // namespace chromeos
