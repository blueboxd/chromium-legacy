// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_H_

#include "base/sequence_checker.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
// TODO(https://crbug.com/967598): Remove this once OAuth2AccessTokenManager's
// separated from OAuth2TokenService.
#include "google_apis/gaia/oauth2_token_service.h"

class OAuth2AccessTokenFetcher;
class OAuth2TokenServiceDelegate;

// Class that manages requests for OAuth2 access tokens.
class OAuth2AccessTokenManager {
 public:
  // TODO(https://crbug.com/967598): Remove |token_service| parameter once
  // OAuth2AccessTokenManager fully manages access tokens independently of
  // OAuth2TokenService and replace |delegate| with
  // OAuth2AccessTokenManagerDelegate.
  explicit OAuth2AccessTokenManager(OAuth2TokenService* token_service,
                                    OAuth2TokenServiceDelegate* delegate);
  virtual ~OAuth2AccessTokenManager();

  OAuth2TokenServiceDelegate* GetDelegate();
  const OAuth2TokenServiceDelegate* GetDelegate() const;

  // Add or remove observers of this token manager.
  void AddDiagnosticsObserver(AccessTokenDiagnosticsObserver* observer);
  void RemoveDiagnosticsObserver(AccessTokenDiagnosticsObserver* observer);

  // Checks in the cache for a valid access token for a specified |account_id|
  // and |scopes|, and if not found starts a request for an OAuth2 access token
  // using the OAuth2 refresh token maintained by this instance for that
  // |account_id|. The caller owns the returned Request.
  // |scopes| is the set of scopes to get an access token for, |consumer| is
  // the object that will be called back with results if the returned request
  // is not deleted.
  std::unique_ptr<OAuth2TokenService::Request> StartRequest(
      const CoreAccountId& account_id,
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenService::Consumer* consumer);

  // This method does the same as |StartRequest| except it uses |client_id| and
  // |client_secret| to identify OAuth client app instead of using
  // Chrome's default values.
  std::unique_ptr<OAuth2TokenService::Request> StartRequestForClient(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenService::Consumer* consumer);

  // This method does the same as |StartRequest| except it uses the
  // URLLoaderfactory given by |url_loader_factory| instead of using the one
  // returned by |GetURLLoaderFactory| implemented by the delegate.
  std::unique_ptr<OAuth2TokenService::Request> StartRequestWithContext(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenService::Consumer* consumer);

  // Fetches an OAuth token for the specified client/scopes.
  void FetchOAuth2Token(
      OAuth2TokenService::RequestImpl* request,
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2TokenService::ScopeSet& scopes);

  // Add a new entry to the cache.
  void RegisterTokenResponse(
      const std::string& client_id,
      const CoreAccountId& account_id,
      const OAuth2TokenService::ScopeSet& scopes,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  // Returns a currently valid OAuth2 access token for the given set of scopes,
  // or NULL if none have been cached. Note the user of this method should
  // ensure no entry with the same |client_scopes| is added before the usage of
  // the returned entry is done.
  const OAuth2AccessTokenConsumer::TokenResponse* GetCachedTokenResponse(
      const OAuth2TokenService::RequestParameters& client_scopes);

  // Clears the internal token cache.
  void ClearCache();

  // Clears all of the tokens belonging to |account_id| from the internal token
  // cache. It does not matter what other parameters, like |client_id| were
  // used to request the tokens.
  void ClearCacheForAccount(const CoreAccountId& account_id);

  // Cancels all requests that are currently in progress.
  void CancelAllRequests();

  // Cancels all requests related to a given |account_id|.
  void CancelRequestsForAccount(const CoreAccountId& account_id);

  void set_max_authorization_token_fetch_retries_for_testing(int max_retries);

  // Returns the current number of pending fetchers matching given params.
  size_t GetNumPendingRequestsForTesting(
      const std::string& client_id,
      const CoreAccountId& account_id,
      const OAuth2TokenService::ScopeSet& scopes) const;

 private:
  // TODO(https://crbug.com/967598): Remove this once |token_cache_| management
  // is moved to OAuth2AccessTokenManager.
  friend class OAuth2TokenService;

  class Fetcher;
  friend class Fetcher;

  OAuth2TokenService::TokenCache& token_cache() { return token_cache_; }

  // Create an access token fetcher for the given account id.
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer);

  // This method does the same as |StartRequestWithContext| except it
  // uses |client_id| and |client_secret| to identify OAuth
  // client app instead of using Chrome's default values.
  std::unique_ptr<OAuth2TokenService::Request> StartRequestForClientWithContext(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenService::Consumer* consumer);

  // Posts a task to fire the Consumer callback with the cached token response.
  void InformConsumerWithCachedTokenResponse(
      const OAuth2AccessTokenConsumer::TokenResponse* token_response,
      OAuth2TokenService::RequestImpl* request,
      const OAuth2TokenService::RequestParameters& client_scopes);

  // Removes an access token for the given set of scopes from the cache.
  // Returns true if the entry was removed, otherwise false.
  bool RemoveCachedTokenResponse(
      const OAuth2TokenService::RequestParameters& client_scopes,
      const std::string& token_to_remove);

  // Called when |fetcher| finishes fetching.
  void OnFetchComplete(Fetcher* fetcher);

  // Called when a number of fetchers need to be canceled.
  void CancelFetchers(std::vector<Fetcher*> fetchers_to_cancel);

  // The cache of currently valid tokens.
  OAuth2TokenService::TokenCache token_cache_;
  // List of observers to notify when access token status changes.
  base::ObserverList<AccessTokenDiagnosticsObserver, true>::Unchecked
      diagnostics_observer_list_;
  // TODO(https://crbug.com/967598): Remove this once OAuth2AccessTokenManager
  // fully manages access tokens independently of OAuth2TokenService.
  OAuth2TokenService* token_service_;
  // TODO(https://crbug.com/967598): Replace it with
  // OAuth2AccessTokenManagerDelegate.
  OAuth2TokenServiceDelegate* delegate_;
  // A map from fetch parameters to a fetcher that is fetching an OAuth2 access
  // token using these parameters.
  std::map<OAuth2TokenService::RequestParameters, std::unique_ptr<Fetcher>>
      pending_fetchers_;
  // Maximum number of retries in fetching an OAuth2 access token.
  static int max_fetch_retry_num_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(OAuth2AccessTokenManager);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_H_
