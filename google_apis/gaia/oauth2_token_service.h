// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_H_
#define GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "google_apis/gaia/oauth2_token_service_observer.h"

namespace network {
class SharedURLLoaderFactory;
}

class OAuth2TokenServiceDelegate;
class OAuth2AccessTokenManager;

// Abstract base class for a service that fetches and caches OAuth2 access
// tokens. Concrete subclasses should implement GetRefreshToken to return
// the appropriate refresh token. Derived services might maintain refresh tokens
// for multiple accounts.
//
// All calls are expected from the UI thread.
//
// To use this service, call StartRequest() with a given set of scopes and a
// consumer of the request results. The consumer is required to outlive the
// request. The request can be deleted. The consumer may be called back
// asynchronously with the fetch results.
//
// - If the consumer is not called back before the request is deleted, it will
//   never be called back.
//   Note in this case, the actual network requests are not canceled and the
//   cache will be populated with the fetched results; it is just the consumer
//   callback that is aborted.
//
// - Otherwise the consumer will be called back with the request and the fetch
//   results.
//
// The caller of StartRequest() owns the returned request and is responsible to
// delete the request even once the callback has been invoked.
class OAuth2TokenService : public OAuth2TokenServiceObserver,
                           public OAuth2AccessTokenManager::Delegate {
 public:
  explicit OAuth2TokenService(
      std::unique_ptr<OAuth2TokenServiceDelegate> delegate);
  ~OAuth2TokenService() override;

  // Overridden from OAuth2AccessTokenManager::Delegate.
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) override;
  bool HasRefreshToken(const CoreAccountId& account_id) const override;
  bool FixRequestErrorIfPossible() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;
  void OnAccessTokenInvalidated(const CoreAccountId& account_id,
                                const std::string& client_id,
                                const std::set<std::string>& scopes,
                                const std::string& access_token) override;
  void OnAccessTokenFetched(const CoreAccountId& account_id,
                            const GoogleServiceAuthError& error) override;

  // Add or remove observers of this token service.
  void AddObserver(OAuth2TokenServiceObserver* observer);
  void RemoveObserver(OAuth2TokenServiceObserver* observer);

  // TODO(https://crbug.com/967598): Remove these APIs once we can use
  // OAuth2AccessTokenManager without OAuth2TokenService.
  // Add or remove observers of access token manager.
  void AddAccessTokenDiagnosticsObserver(
      OAuth2AccessTokenManager::DiagnosticsObserver* observer);
  void RemoveAccessTokenDiagnosticsObserver(
      OAuth2AccessTokenManager::DiagnosticsObserver* observer);

  // Checks in the cache for a valid access token for a specified |account_id|
  // and |scopes|, and if not found starts a request for an OAuth2 access token
  // using the OAuth2 refresh token maintained by this instance for that
  // |account_id|. The caller owns the returned Request.
  // |scopes| is the set of scopes to get an access token for, |consumer| is
  // the object that will be called back with results if the returned request
  // is not deleted. Virtual for mocking.
  // Deprecated. It's moved to OAuth2AccessTokenManager.
  virtual std::unique_ptr<OAuth2AccessTokenManager::Request> StartRequest(
      const CoreAccountId& account_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      OAuth2AccessTokenManager::Consumer* consumer);

  // Try to get refresh token from delegate. If it is accessible (i.e. not
  // empty), return it directly, otherwise start request to get access token.
  // Used for getting tokens to send to Gaia Multilogin endpoint.
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartRequestForMultilogin(
      const CoreAccountId& account_id,
      OAuth2AccessTokenManager::Consumer* consumer);

  // This method does the same as |StartRequest| except it uses |client_id| and
  // |client_secret| to identify OAuth client app instead of using
  // Chrome's default values.
  // Deprecated. It's moved to OAuth2AccessTokenManager.
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartRequestForClient(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      OAuth2AccessTokenManager::Consumer* consumer);

  // This method does the same as |StartRequest| except it uses the
  // URLLoaderfactory given by |url_loader_factory| instead of using the one
  // returned by Delegate::GetURLLoaderFactory().
  // Deprecated. It's moved to OAuth2AccessTokenManager.
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartRequestWithContext(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      OAuth2AccessTokenManager::Consumer* consumer);

  // Returns true iff all credentials have been loaded from disk.
  bool AreAllCredentialsLoaded() const;

  void set_all_credentials_loaded_for_testing(bool loaded) {
    all_credentials_loaded_ = loaded;
  }

  // Lists account IDs of all accounts with a refresh token maintained by this
  // instance.
  // Note: For each account returned by |GetAccounts|, |RefreshTokenIsAvailable|
  // will return true.
  // Note: If tokens have not been fully loaded yet, an empty list is returned.
  std::vector<CoreAccountId> GetAccounts() const;

  // Returns true if a refresh token exists for |account_id|. If false, calls to
  // |StartRequest| will result in a Consumer::OnGetTokenFailure callback.
  // Note: This will return |true| if and only if |account_id| is contained in
  // the list returned by |GetAccounts|.
  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const;

  // Returns true if a refresh token exists for |account_id| and it is in a
  // persistent error state.
  bool RefreshTokenHasError(const CoreAccountId& account_id) const;

  // Returns the auth error associated with |account_id|. Only persistent errors
  // will be returned.
  GoogleServiceAuthError GetAuthError(const CoreAccountId& account_id) const;

  // Mark an OAuth2 |access_token| issued for |account_id| and |scopes| as
  // invalid. This should be done if the token was received from this class,
  // but was not accepted by the server (e.g., the server returned
  // 401 Unauthorized). The token will be removed from the cache for the given
  // scopes.
  // Deprecated. It's moved to OAuth2AccessTokenManager.
  void InvalidateAccessToken(const CoreAccountId& account_id,
                             const OAuth2AccessTokenManager::ScopeSet& scopes,
                             const std::string& access_token);

  // Removes token from cache (if it is cached) and calls
  // InvalidateTokenForMultilogin method of the delegate. This should be done if
  // the token was received from this class, but was not accepted by the server
  // (e.g., the server returned 401 Unauthorized).
  virtual void InvalidateTokenForMultilogin(const CoreAccountId& failed_account,
                                            const std::string& token);

  // Deprecated. It's moved to OAuth2AccessTokenManager.
  void set_max_authorization_token_fetch_retries_for_testing(int max_retries);
  // Returns the current number of pending fetchers matching given params.
  // Deprecated. It's moved to OAuth2AccessTokenManager.
  size_t GetNumPendingRequestsForTesting(
      const std::string& client_id,
      const CoreAccountId& account_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes) const;
  // Override |token_manager_| for testing.
  void OverrideAccessTokenManagerForTesting(
      std::unique_ptr<OAuth2AccessTokenManager> token_manager);

  OAuth2TokenServiceDelegate* GetDelegate();
  const OAuth2TokenServiceDelegate* GetDelegate() const;

  OAuth2AccessTokenManager* GetAccessTokenManager();

  // TODO(https://crbug.com/967598): Remove this. It's opened only for
  // OAuth2TokenServiceTest.
  int GetTokenCacheCount();

  const base::ObserverList<OAuth2AccessTokenManager::DiagnosticsObserver,
                           true>::Unchecked&
  GetAccessTokenDiagnosticsObservers();

 protected:
  // TODO(https://crbug.com/967598): Remove this once OAuth2AccessTokenManager
  // fully manages access tokens independently of OAuth2TokenService.
  friend class OAuth2AccessTokenManager;

  // OAuth2TokenServiceObserver:
  void OnRefreshTokensLoaded() override;

  // Add a new entry to the cache.
  // Subclasses can override if there are implementation-specific reasons
  // that an access token should ever not be cached.
  virtual void RegisterTokenResponse(
      const std::string& client_id,
      const CoreAccountId& account_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  // Clears the internal token cache.
  void ClearCache();

  // Clears all of the tokens belonging to |account_id| from the internal token
  // cache. It does not matter what other parameters, like |client_id| were
  // used to request the tokens.
  void ClearCacheForAccount(const CoreAccountId& account_id);

  // Cancels all requests that are currently in progress. Virtual so it can be
  // overridden for tests.
  // Deprecated. It's moved to OAuth2AccessTokenManager.
  virtual void CancelAllRequests();

  // Cancels all requests related to a given |account_id|. Virtual so it can be
  // overridden for tests.
  // Deprecated. It's moved to OAuth2AccessTokenManager.
  virtual void CancelRequestsForAccount(const CoreAccountId& account_id);

 private:
  friend class OAuth2TokenServiceDelegate;

  std::unique_ptr<OAuth2TokenServiceDelegate> delegate_;

  // The depth of batch changes.
  int batch_change_depth_;

  // Whether all credentials have been loaded.
  bool all_credentials_loaded_;

  std::unique_ptr<OAuth2AccessTokenManager> token_manager_;

  FRIEND_TEST_ALL_PREFIXES(OAuth2TokenServiceTest, RequestParametersOrderTest);
  FRIEND_TEST_ALL_PREFIXES(OAuth2TokenServiceTest,
                           SameScopesRequestedForDifferentClients);
  FRIEND_TEST_ALL_PREFIXES(OAuth2TokenServiceTest, UpdateClearsCache);

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenService);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_H_
