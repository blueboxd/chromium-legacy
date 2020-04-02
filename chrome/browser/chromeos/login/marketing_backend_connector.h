// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MARKETING_BACKEND_CONNECTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MARKETING_BACKEND_CONNECTOR_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace chromeos {

class MarketingBackendConnector
    : public base::RefCountedThreadSafe<MarketingBackendConnector> {
 public:
  MarketingBackendConnector(const MarketingBackendConnector&) = delete;
  MarketingBackendConnector& operator=(const MarketingBackendConnector&) =
      delete;

  // A fire and forget method to be called on the marketing opt-in screen.
  // It will create an instance of  MarketingBackendConnectorthat calls the
  // backend to update the user preferences.
  static void UpdateChromebookEmailPreferences();

  enum class UmaEvent {
    // Differentiate between users who have a default opt-in vs default opt-out
    USER_OPTED_IN_WHEN_DEFAULT_IS_OPT_IN,
    USER_OPTED_IN_WHEN_DEFAULT_IS_OPT_OUT,
    USER_OPTED_OUT_WHEN_DEFAULT_IS_OPT_IN,
    USER_OPTED_OUT_WHEN_DEFAULT_IS_OPT_OUT,
    // Successfully set the user preference on the server
    SUCCESS,
    // Possible errors to keep track of.
    ERROR_SERVER_INTERNAL,
    ERROR_REQUEST_TIMEOUT,
    ERROR_AUTH,
    ERROR_OTHER
  };

 private:
  explicit MarketingBackendConnector(Profile* user_profile);

  virtual ~MarketingBackendConnector();

  // Sends a request to the server to subscribe the user to all campaigns.
  void PerformRequest();

  // Starts the token fetch process.
  void StartTokenFetch();

  // Handles the token fetch response.
  void OnAccessTokenRequestCompleted(GoogleServiceAuthError error,
                                     signin::AccessTokenInfo access_token_info);

  // Sets the authentication token in the request header and starts the request
  void SetTokenAndStartRequest();

  // Handles responses from the SimpleURLLoader
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnSimpleLoaderCompleteInternal(int response_code,
                                      const std::string& data);

  // Generates the content of the request to be sent based on the country and
  // the language.
  std::string GetRequestContent();

  // Internal
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::string access_token_;
  Profile* profile_ = nullptr;

  friend class base::RefCountedThreadSafe<MarketingBackendConnector>;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MARKETING_BACKEND_CONNECTOR_H_
