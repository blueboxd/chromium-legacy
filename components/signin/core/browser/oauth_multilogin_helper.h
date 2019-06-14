// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_OAUTH_MULTILOGIN_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_OAUTH_MULTILOGIN_HELPER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

class GaiaAuthFetcher;
class GoogleServiceAuthError;
class OAuth2TokenService;
class SigninClient;

namespace signin {

class OAuthMultiloginTokenFetcher;
enum class SetAccountsInCookieResult;

// This is a helper class that drives the OAuth multilogin process.
// The main steps are:
// - fetch access tokens with login scope,
// - call the oauth multilogin endpoint,
// - get the cookies from the response body, and set them in the cookie manager.
// It is safe to delete this object from within the callbacks.
class OAuthMultiloginHelper : public GaiaAuthConsumer {
 public:
  OAuthMultiloginHelper(
      SigninClient* signin_client,
      OAuth2TokenService* token_service,
      const std::vector<std::string>& account_ids,
      const std::string& external_cc_result,
      base::OnceCallback<void(signin::SetAccountsInCookieResult)> callback);

  ~OAuthMultiloginHelper() override;

 private:
  // Starts fetching tokens with OAuthMultiloginTokenFetcher.
  void StartFetchingTokens();

  // Callbacks for OAuthMultiloginTokenFetcher.
  void OnAccessTokensSuccess(
      const std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>&
          token_id_pairs);
  void OnAccessTokensFailure(const GoogleServiceAuthError& error);

  // Actual call to the multilogin endpoint.
  void StartFetchingMultiLogin();

  // Overridden from GaiaAuthConsumer.
  void OnOAuthMultiloginFinished(const OAuthMultiloginResult& result) override;

  // Starts setting parsed cookies in browser.
  void StartSettingCookies(const OAuthMultiloginResult& result);

  // Callback for CookieManager::SetCanonicalCookie.
  void OnCookieSet(const std::string& cookie_name,
                   const std::string& cookie_domain,
                   net::CanonicalCookie::CookieInclusionStatus status);

  SigninClient* signin_client_;
  OAuth2TokenService* token_service_;

  int fetcher_retries_ = 0;

  // Account ids to set in the cookie.
  const std::vector<std::string> account_ids_;
  // See GaiaCookieManagerService::ExternalCcResultFetcher for details.
  const std::string external_cc_result_;
  // Access tokens, in the same order as the account ids.
  std::vector<GaiaAuthFetcher::MultiloginTokenIDPair> token_id_pairs_;

  base::OnceCallback<void(signin::SetAccountsInCookieResult)> callback_;
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  std::unique_ptr<OAuthMultiloginTokenFetcher> token_fetcher_;

  // List of pairs (cookie name and cookie domain) that have to be set in
  // cookie jar.
  std::set<std::pair<std::string, std::string>> cookies_to_set_;

  base::WeakPtrFactory<OAuthMultiloginHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OAuthMultiloginHelper);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_OAUTH_MULTILOGIN_HELPER_H_
