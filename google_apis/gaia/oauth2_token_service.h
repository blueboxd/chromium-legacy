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
class OAuth2TokenService : public OAuth2AccessTokenManager::Delegate {
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

  // TODO(https://crbug.com/967598): Remove these APIs once we can use
  // OAuth2AccessTokenManager without OAuth2TokenService.
  // Add or remove observers of access token manager.
  void AddAccessTokenDiagnosticsObserver(
      OAuth2AccessTokenManager::DiagnosticsObserver* observer);
  void RemoveAccessTokenDiagnosticsObserver(
      OAuth2AccessTokenManager::DiagnosticsObserver* observer);

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

 private:
  // TODO(https://crbug.com/967598): Completely merge this class into
  // ProfileOAuth2TokenService.
  friend class ProfileOAuth2TokenService;
  friend class OAuth2TokenServiceDelegate;

  std::unique_ptr<OAuth2TokenServiceDelegate> delegate_;

  // The depth of batch changes.
  int batch_change_depth_;

  std::unique_ptr<OAuth2AccessTokenManager> token_manager_;

  FRIEND_TEST_ALL_PREFIXES(OAuth2TokenServiceTest, RequestParametersOrderTest);
  FRIEND_TEST_ALL_PREFIXES(OAuth2TokenServiceTest,
                           SameScopesRequestedForDifferentClients);
  FRIEND_TEST_ALL_PREFIXES(OAuth2TokenServiceTest, UpdateClearsCache);

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenService);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_H_
