// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_token_service_delegate.h"

#include "google_apis/gaia/oauth2_token_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
const char OAuth2TokenServiceDelegate::kInvalidRefreshToken[] =
    "invalid_refresh_token";

OAuth2TokenServiceDelegate::ScopedBatchChange::ScopedBatchChange(
    OAuth2TokenServiceDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  delegate_->StartBatchChanges();
}

OAuth2TokenServiceDelegate::ScopedBatchChange::~ScopedBatchChange() {
  delegate_->EndBatchChanges();
}

OAuth2TokenServiceDelegate::OAuth2TokenServiceDelegate()
    : batch_change_depth_(0) {}

OAuth2TokenServiceDelegate::~OAuth2TokenServiceDelegate() {
}

bool OAuth2TokenServiceDelegate::ValidateAccountId(
    const CoreAccountId& account_id) const {
  bool valid = !account_id.empty();

  // If the account is given as an email, make sure its a canonical email.
  // Note that some tests don't use email strings as account id, and after
  // the gaia id migration it won't be an email.  So only check for
  // canonicalization if the account_id is suspected to be an email.
  if (account_id.id.find('@') != std::string::npos &&
      gaia::CanonicalizeEmail(account_id.id) != account_id.id) {
    valid = false;
  }

  DCHECK(valid);
  return valid;
}

void OAuth2TokenServiceDelegate::AddObserver(
    OAuth2TokenServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void OAuth2TokenServiceDelegate::RemoveObserver(
    OAuth2TokenServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void OAuth2TokenServiceDelegate::StartBatchChanges() {
  ++batch_change_depth_;
}

void OAuth2TokenServiceDelegate::EndBatchChanges() {
  --batch_change_depth_;
  DCHECK_LE(0, batch_change_depth_);
  if (batch_change_depth_ == 0) {
    for (auto& observer : observer_list_)
      observer.OnEndBatchChanges();
  }
}

void OAuth2TokenServiceDelegate::FireRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  DCHECK(!account_id.empty());
  for (auto& observer : observer_list_)
    observer.OnRefreshTokenAvailable(account_id);
}

void OAuth2TokenServiceDelegate::FireRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  DCHECK(!account_id.empty());
  for (auto& observer : observer_list_)
    observer.OnRefreshTokenRevoked(account_id);
}

void OAuth2TokenServiceDelegate::FireRefreshTokensLoaded() {
  for (auto& observer : observer_list_)
    observer.OnRefreshTokensLoaded();
}

void OAuth2TokenServiceDelegate::FireAuthErrorChanged(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK(!account_id.empty());
  for (auto& observer : observer_list_)
    observer.OnAuthErrorChanged(account_id, error);
}

std::string OAuth2TokenServiceDelegate::GetTokenForMultilogin(
    const CoreAccountId& account_id) const {
  return std::string();
}

scoped_refptr<network::SharedURLLoaderFactory>
OAuth2TokenServiceDelegate::GetURLLoaderFactory() const {
  return nullptr;
}

GoogleServiceAuthError OAuth2TokenServiceDelegate::GetAuthError(
    const CoreAccountId& account_id) const {
  return GoogleServiceAuthError::AuthErrorNone();
}

std::vector<CoreAccountId> OAuth2TokenServiceDelegate::GetAccounts() const {
  return std::vector<CoreAccountId>();
}

const net::BackoffEntry* OAuth2TokenServiceDelegate::BackoffEntry() const {
  return nullptr;
}

void OAuth2TokenServiceDelegate::LoadCredentials(
    const CoreAccountId& primary_account_id) {
  NOTREACHED() << "OAuth2TokenServiceDelegate does not load credentials. "
                  "Subclasses that need to load credentials must provide "
                  "an implemenation of this method";
}

void OAuth2TokenServiceDelegate::ExtractCredentials(
    OAuth2TokenService* to_service,
    const CoreAccountId& account_id) {
  NOTREACHED();
}

bool OAuth2TokenServiceDelegate::FixRequestErrorIfPossible() {
  return false;
}
