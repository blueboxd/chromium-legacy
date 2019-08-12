// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request.h"
#include "url/gurl.h"

class GoogleServiceAuthError;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace password_manager {

class LeakDetectionDelegateInterface;
struct SingleLookupResponse;

// Performs a leak-check for {username, password} for Chrome signed-in users.
class AuthenticatedLeakCheck : public LeakDetectionCheck {
 public:
  AuthenticatedLeakCheck(
      LeakDetectionDelegateInterface* delegate,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AuthenticatedLeakCheck() override;

  // Returns true if there is a Google account to use for the leak detection
  // check. Otherwise, instantiating the class is pointless.
  static bool HasAccountForRequest(
      const signin::IdentityManager* identity_manager);

  // LeakDetectionCheck:
  void Start(const GURL& url,
             base::StringPiece16 username,
             base::StringPiece16 password) override;

#if defined(UNIT_TEST)
  const std::string& access_token() const { return access_token_; }
#endif  // defined(UNIT_TEST)

 private:
  // Called when the token request is done.
  void OnAccessTokenRequestCompleted(GoogleServiceAuthError error,
                                     signin::AccessTokenInfo access_token_info);

  // Called when the single leak lookup request is done. |response| is null in
  // case of an invalid server response, or contains a valid
  // SingleLookupResponse instance otherwise.
  void OnLookupSingleLeakResponse(
      std::unique_ptr<SingleLookupResponse> response);

  // Delegate for the instance. Should outlive |this|.
  LeakDetectionDelegateInterface* delegate_;
  // Identity manager for the profile.
  signin::IdentityManager* identity_manager_;
  // URL loader factory required for the network request to the identity
  // endpoint.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Actual request for the needed token.
  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher_;
  // Class used to initiate a request to the identity leak lookup endpoint. This
  // is only instantiated if a valid |access_token_| could be obtained.
  std::unique_ptr<LeakDetectionRequest> request_;
  // |url| passed to Start().
  GURL url_;
  // |username| passed to Start().
  std::string username_;
  // |password| passed to Start().
  std::string password_;
  // The token to be used for request.
  std::string access_token_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_
