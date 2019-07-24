// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observer.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "google_apis/gaia/oauth2_token_service_observer.h"
#include "services/identity/public/cpp/scope_set.h"

namespace network {
class SharedURLLoaderFactory;
}

class GoogleServiceAuthError;
class ProfileOAuth2TokenService;

namespace signin {
struct AccessTokenInfo;

// Helper class to ease the task of obtaining an OAuth2 access token for a
// given account.
// May only be used on the UI thread.
class AccessTokenFetcher : public OAuth2TokenServiceObserver,
                           public OAuth2AccessTokenManager::Consumer {
 public:
  // Specifies how this instance should behave:
  // |kImmediate|: Makes one-shot immediate request.
  // |kWaitUntilRefreshTokenAvailable|: Waits for the account to have a refresh
  // token before making the request.
  // Note that using |kWaitUntilRefreshTokenAvailable| can result in waiting
  // forever if the user is not signed in and doesn't sign in.
  enum class Mode { kImmediate, kWaitUntilRefreshTokenAvailable };

  // Callback for when a request completes (successful or not). On successful
  // requests, |error| is NONE and |access_token_info| contains info of the
  // obtained OAuth2 access token. On failed requests, |error| contains the
  // actual error and |access_token_info| is empty.
  // NOTE: At the time that this method is invoked, it is safe for the client to
  // destroy the AccessTokenFetcher instance that is invoking this callback.
  using TokenCallback =
      base::OnceCallback<void(GoogleServiceAuthError error,
                              AccessTokenInfo access_token_info)>;

  // Instantiates a fetcher and immediately starts the process of obtaining an
  // OAuth2 access token for |account_id| and |scopes|. The |callback| is called
  // once the request completes (successful or not). If the AccessTokenFetcher
  // is destroyed before the process completes, the callback is not called.
  AccessTokenFetcher(const CoreAccountId& account_id,
                     const std::string& oauth_consumer_name,
                     ProfileOAuth2TokenService* token_service,
                     const identity::ScopeSet& scopes,
                     TokenCallback callback,
                     Mode mode);

  // Instantiates a fetcher and immediately starts the process of obtaining an
  // OAuth2 access token for |account_id| and |scopes|, allowing clients to pass
  // a |url_loader_factory| of their choice. The |callback| is called
  // once the request completes (successful or not). If the AccessTokenFetcher
  // is destroyed before the process completes, the callback is not called.
  AccessTokenFetcher(
      const CoreAccountId& account_id,
      const std::string& oauth_consumer_name,
      ProfileOAuth2TokenService* token_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const identity::ScopeSet& scopes,
      TokenCallback callback,
      Mode mode);

  // Instantiates a fetcher and immediately starts the process of obtaining an
  // OAuth2 access token for |account_id| and |scopes| using both the
  // |client_id| and |client_secret| to identify the OAuth client app.
  AccessTokenFetcher(const CoreAccountId& account_id,
                     const std::string client_id,
                     const std::string client_secret,
                     const std::string& oauth_consumer_name,
                     ProfileOAuth2TokenService* token_service,
                     const identity::ScopeSet& scopes,
                     TokenCallback callback,
                     Mode mode);

  ~AccessTokenFetcher() override;

 private:
  AccessTokenFetcher(
      const CoreAccountId& account_id,
      const std::string client_id,
      const std::string client_secret,
      const std::string& oauth_consumer_name,
      ProfileOAuth2TokenService* token_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const identity::ScopeSet& scopes,
      TokenCallback callback,
      Mode mode);

  // Returns true iff a refresh token is available for |account_id_|. Should
  // only be called in mode |kWaitUntilAvailable|.
  bool IsRefreshTokenAvailable() const;

  void StartAccessTokenRequest();

  // OAuth2TokenServiceObserver implementation.
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override;

  // OAuth2AccessTokenManager::Consumer implementation.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Invokes |callback_| with (|error|, |access_token_info|). Per the contract
  // of this class, it is allowed for clients to delete this object as part of
  // the invocation of |callback_|. Hence, this object must assume that it is
  // dead after invoking this method and must not run any more code.
  void RunCallbackAndMaybeDie(GoogleServiceAuthError error,
                              AccessTokenInfo access_token_info);

  const CoreAccountId account_id_;
  const std::string client_id_;
  const std::string client_secret_;
  ProfileOAuth2TokenService* token_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const identity::ScopeSet scopes_;
  const Mode mode_;

  // NOTE: This callback should only be invoked from |RunCallbackAndMaybeDie|,
  // as invoking it has the potential to destroy this object per this class's
  // contract.
  TokenCallback callback_;

  ScopedObserver<ProfileOAuth2TokenService, AccessTokenFetcher>
      token_service_observer_;

  std::unique_ptr<OAuth2AccessTokenManager::Request> access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(AccessTokenFetcher);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_FETCHER_H_
