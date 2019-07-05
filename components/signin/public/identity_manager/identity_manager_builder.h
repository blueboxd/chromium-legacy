// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_BUILDER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_BUILDER_H_

#include <memory>

class AccountTrackerService;
class PrefService;
class ProfileOAuth2TokenService;
class SigninClient;

namespace image_fetcher {
class ImageDecoder;
}

namespace signin {
enum class AccountConsistencyMethod;
}

namespace identity {
class IdentityManager;

struct IdentityManagerBuildParams {
  IdentityManagerBuildParams();
  ~IdentityManagerBuildParams();

  signin::AccountConsistencyMethod account_consistency;
  std::unique_ptr<AccountTrackerService> account_tracker_service;
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder;
  PrefService* local_state;
  PrefService* pref_service;
  SigninClient* signin_client;
  std::unique_ptr<ProfileOAuth2TokenService> token_service;
};

// Builds an IdentityManager instance from the supplied embedder-level
// dependencies.
std::unique_ptr<IdentityManager> BuildIdentityManager(
    IdentityManagerBuildParams* params);

}  // namespace identity

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_BUILDER_H_
