// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ambient/ambient_client_impl.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr char kPhotosOAuthScope[] = "https://www.googleapis.com/auth/photos";

const user_manager::User* GetActiveUser() {
  return user_manager::UserManager::Get()->GetActiveUser();
}

Profile* GetProfileForActiveUser() {
  const user_manager::User* const active_user = GetActiveUser();
  DCHECK(active_user);
  return chromeos::ProfileHelper::Get()->GetProfileByUser(active_user);
}

}  // namespace

AmbientClientImpl::AmbientClientImpl() = default;

AmbientClientImpl::~AmbientClientImpl() = default;

bool AmbientClientImpl::IsAmbientModeAllowedForActiveUser() {
  if (chromeos::DemoSession::IsDeviceInDemoMode())
    return false;

  const user_manager::User* const active_user = GetActiveUser();
  if (!active_user || !active_user->HasGaiaAccount())
    return false;

  auto* profile = GetProfileForActiveUser();
  if (!profile)
    return false;

  if (!profile->IsRegularProfile())
    return false;

  return true;
}

void AmbientClientImpl::RequestAccessToken(GetAccessTokenCallback callback) {
  auto* profile = GetProfileForActiveUser();
  DCHECK(profile);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager);

  CoreAccountInfo account_info = identity_manager->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);

  const signin::ScopeSet scopes{kPhotosOAuthScope};
  // TODO(b/148463064): Handle retry refresh token and multiple requests.
  // Currently only one request is allowed.
  DCHECK(!access_token_fetcher_);
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_info.account_id, /*oauth_consumer_name=*/"ChromeOS_AmbientMode",
      scopes,
      base::BindOnce(&AmbientClientImpl::GetAccessToken,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     account_info.gaia),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

scoped_refptr<network::SharedURLLoaderFactory>
AmbientClientImpl::GetURLLoaderFactory() {
  auto* profile = GetProfileForActiveUser();
  DCHECK(profile);

  return profile->GetURLLoaderFactory();
}

void AmbientClientImpl::GetAccessToken(
    GetAccessTokenCallback callback,
    const std::string& gaia_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  // It's safe to delete AccessTokenFetcher from inside its own callback.
  access_token_fetcher_.reset();

  if (error.state() == GoogleServiceAuthError::NONE) {
    std::move(callback).Run(gaia_id, access_token_info.token,
                            access_token_info.expiration_time);
  } else {
    LOG(ERROR) << "Failed to retrieve token, error: " << error.ToString();
    std::move(callback).Run(/*gaia_id=*/std::string(),
                            /*access_token=*/std::string(),
                            /*expiration_time=*/base::Time::Now());
  }
}
