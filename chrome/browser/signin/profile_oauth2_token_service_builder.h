// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_
#define CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_

#include <memory>

class AccountTrackerService;
class IdentityManagerFactory;
class PrefService;
class ProfileOAuth2TokenService;
class SigninClient;

namespace signin {
enum class AccountConsistencyMethod;
}

namespace content {
class BrowserContext;
}

namespace network {
class NetworkConnectionTracker;
}

class ProfileOAuth2TokenServiceBuilder {
 private:
  // Builds a ProfileOAuth2TokenService instance for use by IdentityManager.
  static std::unique_ptr<ProfileOAuth2TokenService> BuildInstanceFor(
      content::BrowserContext* context,
      PrefService* pref_service,
      AccountTrackerService* account_tracker_service,
      network::NetworkConnectionTracker* network_connection_tracker,
      signin::AccountConsistencyMethod account_consistency,
      SigninClient* signin_client);

  friend IdentityManagerFactory;
};
#endif  // CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_
