// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"

#include <utility>

#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {
namespace {

constexpr char kAPIScope[] = "https://www.googleapis.com/auth/userinfo.id";

// Returns a Google account that can be used for getting a token.
CoreAccountId GetAccountForRequest(
    const signin::IdentityManager* identity_manager) {
  CoreAccountInfo result = identity_manager->GetUnconsentedPrimaryAccountInfo();
  if (result.IsEmpty()) {
    std::vector<CoreAccountInfo> all_accounts =
        identity_manager->GetAccountsWithRefreshTokens();
    if (!all_accounts.empty())
      result = all_accounts.front();
  }
  return result.account_id;
}

}  // namespace

AuthenticatedLeakCheck::AuthenticatedLeakCheck(
    LeakDetectionDelegateInterface* delegate,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : delegate_(delegate),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(identity_manager_);
  DCHECK(delegate_);
}

AuthenticatedLeakCheck::~AuthenticatedLeakCheck() = default;

// static
bool AuthenticatedLeakCheck::HasAccountForRequest(
    const signin::IdentityManager* identity_manager) {
  // On desktop HasUnconsentedPrimaryAccount() will always return something if
  // the user is signed in.
  // On Android it will be empty if the user isn't syncing. Thus,
  // GetAccountsWithRefreshTokens() check is necessary.
  return identity_manager &&
         (identity_manager->HasUnconsentedPrimaryAccount() ||
          !identity_manager->GetAccountsWithRefreshTokens().empty());
}

void AuthenticatedLeakCheck::Start(const GURL& url,
                                   base::StringPiece16 username,
                                   base::StringPiece16 password) {
  token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      GetAccountForRequest(identity_manager_),
      /*consumer_name=*/"leak_detection_service", {kAPIScope},
      base::BindOnce(&AuthenticatedLeakCheck::OnAccessTokenRequestCompleted,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void AuthenticatedLeakCheck::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE) {
    // The fetcher successfully obtained an access token.
    access_token_ = std::move(access_token_info.token);
    DVLOG(0) << "Token=" << access_token_;
  } else {
    DLOG(ERROR) << "Token request error: " << error.error_message();
    delegate_->OnError(LeakDetectionError::kTokenRequestFailure);
  }
}

}  // namespace password_manager
