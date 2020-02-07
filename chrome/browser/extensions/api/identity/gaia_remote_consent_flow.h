// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_GAIA_REMOTE_CONSENT_FLOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_GAIA_REMOTE_CONSENT_FLOW_H_

#include "base/callback_list.h"

#include "base/macros.h"
#include "chrome/browser/extensions/api/identity/extension_token_key.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

namespace extensions {

class GaiaRemoteConsentFlow
    : public WebAuthFlow::Delegate,
      public signin::AccountsCookieMutator::PartitionDelegate {
 public:
  enum Failure {
    WINDOW_CLOSED,
    LOAD_FAILED,
    SET_ACCOUNTS_IN_COOKIE_FAILED,
    INVALID_CONSENT_RESULT,
    NO_GRANT
  };

  class Delegate {
   public:
    virtual ~Delegate();
    // Called when the flow ends without getting the user consent.
    virtual void OnGaiaRemoteConsentFlowFailed(Failure failure) = 0;
    // Called when the user gives the approval via the OAuth2 remote consent
    // screen.
    virtual void OnGaiaRemoteConsentFlowApproved(
        const std::string& consent_result,
        const std::string& gaia_id) = 0;
  };

  GaiaRemoteConsentFlow(Delegate* delegate,
                        Profile* profile,
                        const ExtensionTokenKey& token_key,
                        const RemoteConsentResolutionData& resolution_data);
  ~GaiaRemoteConsentFlow() override;

  GaiaRemoteConsentFlow(const GaiaRemoteConsentFlow& other) = delete;
  GaiaRemoteConsentFlow& operator=(const GaiaRemoteConsentFlow& other) = delete;

  // Starts the flow by setting accounts in cookie.
  void Start();

  // Set accounts in cookie completion callback.
  void OnSetAccountsComplete(signin::SetAccountsInCookieResult result);

  // setConsentResult() JavaScript callback.
  void OnConsentResultSet(const std::string& consent_result,
                          const std::string& window_id);

  // WebAuthFlow::Delegate implementation.
  void OnAuthFlowFailure(WebAuthFlow::Failure failure) override;

  // AccountsCookieMutator::PartitionDelegate:
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcherForPartition(
      GaiaAuthConsumer* consumer) override;
  network::mojom::CookieManager* GetCookieManagerForPartition() override;

  void SetWebAuthFlowForTesting(std::unique_ptr<WebAuthFlow> web_auth_flow);

 private:
  Delegate* delegate_;
  Profile* profile_;
  CoreAccountId account_id_;
  RemoteConsentResolutionData resolution_data_;
  std::unique_ptr<WebAuthFlow> web_flow_;
  std::unique_ptr<signin::AccountsCookieMutator::SetAccountsInCookieTask>
      set_accounts_in_cookie_task_;
  std::unique_ptr<base::CallbackList<void(const std::string&,
                                          const std::string&)>::Subscription>
      identity_api_set_consent_result_subscription_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_GAIA_REMOTE_CONSENT_FLOW_H_
