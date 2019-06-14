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
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_token_service_observer.h"

namespace network {
class SharedURLLoaderFactory;
}

class GoogleServiceAuthError;
class OAuth2AccessTokenFetcher;
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
class OAuth2TokenService : public OAuth2TokenServiceObserver {
 public:
  // A set of scopes in OAuth2 authentication.
  typedef std::set<std::string> ScopeSet;

  // Class representing a request that fetches an OAuth2 access token.
  class Request {
   public:
    virtual ~Request();
    virtual std::string GetAccountId() const = 0;
   protected:
    Request();
  };

  // Class representing the consumer of a Request passed to |StartRequest|,
  // which will be called back when the request completes.
  class Consumer {
   public:
    explicit Consumer(const std::string& id);
    virtual ~Consumer();

    std::string id() const { return id_; }

    // |request| is a Request that is started by this consumer and has
    // completed.
    virtual void OnGetTokenSuccess(
        const Request* request,
        const OAuth2AccessTokenConsumer::TokenResponse& token_response) = 0;
    virtual void OnGetTokenFailure(const Request* request,
                                   const GoogleServiceAuthError& error) = 0;
   private:
    std::string id_;
  };

  // Classes that want to monitor status of access token and access token
  // request should implement this interface and register with the
  // AddDiagnosticsObserver() call.
  class DiagnosticsObserver {
   public:
    // Called when receiving request for access token.
    virtual void OnAccessTokenRequested(const CoreAccountId& account_id,
                                        const std::string& consumer_id,
                                        const ScopeSet& scopes) {}
    // Called when access token fetching finished successfully or
    // unsuccessfully. |expiration_time| are only valid with
    // successful completion.
    virtual void OnFetchAccessTokenComplete(const CoreAccountId& account_id,
                                            const std::string& consumer_id,
                                            const ScopeSet& scopes,
                                            GoogleServiceAuthError error,
                                            base::Time expiration_time) {}
    // Called when an access token was removed.
    virtual void OnAccessTokenRemoved(const CoreAccountId& account_id,
                                      const ScopeSet& scopes) {}

    // Caled when a new refresh token is available. Contains diagnostic
    // information about the source of the update credentials operation.
    virtual void OnRefreshTokenAvailableFromSource(
        const CoreAccountId& account_id,
        bool is_refresh_token_valid,
        const std::string& source) {}

    // Called when a refreh token is revoked. Contains diagnostic information
    // about the source that initiated the revokation operation.
    virtual void OnRefreshTokenRevokedFromSource(
        const CoreAccountId& account_id,
        const std::string& source) {}
  };

  // The parameters used to fetch an OAuth2 access token.
  struct RequestParameters {
    RequestParameters(const std::string& client_id,
                      const std::string& account_id,
                      const ScopeSet& scopes);
    RequestParameters(const RequestParameters& other);
    ~RequestParameters();
    bool operator<(const RequestParameters& params) const;

    // OAuth2 client id.
    std::string client_id;
    // Account id for which the request is made.
    std::string account_id;
    // URL scopes for the requested access token.
    ScopeSet scopes;
  };
  typedef std::map<RequestParameters, OAuth2AccessTokenConsumer::TokenResponse>
      TokenCache;

  explicit OAuth2TokenService(
      std::unique_ptr<OAuth2TokenServiceDelegate> delegate);
  ~OAuth2TokenService() override;

  // Add or remove observers of this token service.
  void AddObserver(OAuth2TokenServiceObserver* observer);
  void RemoveObserver(OAuth2TokenServiceObserver* observer);

  // Add or remove observers of this token service.
  void AddDiagnosticsObserver(DiagnosticsObserver* observer);
  void RemoveDiagnosticsObserver(DiagnosticsObserver* observer);

  // Checks in the cache for a valid access token for a specified |account_id|
  // and |scopes|, and if not found starts a request for an OAuth2 access token
  // using the OAuth2 refresh token maintained by this instance for that
  // |account_id|. The caller owns the returned Request.
  // |scopes| is the set of scopes to get an access token for, |consumer| is
  // the object that will be called back with results if the returned request
  // is not deleted. Virtual for mocking.
  virtual std::unique_ptr<Request> StartRequest(const CoreAccountId& account_id,
                                                const ScopeSet& scopes,
                                                Consumer* consumer);

  // Try to get refresh token from delegate. If it is accessible (i.e. not
  // empty), return it directly, otherwise start request to get access token.
  // Used for getting tokens to send to Gaia Multilogin endpoint.
  std::unique_ptr<OAuth2TokenService::Request> StartRequestForMultilogin(
      const CoreAccountId& account_id,
      OAuth2TokenService::Consumer* consumer);

  // This method does the same as |StartRequest| except it uses |client_id| and
  // |client_secret| to identify OAuth client app instead of using
  // Chrome's default values.
  std::unique_ptr<Request> StartRequestForClient(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const std::string& client_secret,
      const ScopeSet& scopes,
      Consumer* consumer);

  // This method does the same as |StartRequest| except it uses the
  // URLLoaderfactory given by |url_loader_factory| instead of using the one
  // returned by |GetURLLoaderFactory| implemented by derived classes.
  std::unique_ptr<Request> StartRequestWithContext(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const ScopeSet& scopes,
      Consumer* consumer);

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
  void InvalidateAccessToken(const CoreAccountId& account_id,
                             const ScopeSet& scopes,
                             const std::string& access_token);

  // Like |InvalidateToken| except is uses |client_id| to identity OAuth2 client
  // app that issued the request instead of Chrome's default values.
  void InvalidateAccessTokenForClient(const CoreAccountId& account_id,
                                      const std::string& client_id,
                                      const ScopeSet& scopes,
                                      const std::string& access_token);

  // Removes token from cache (if it is cached) and calls
  // InvalidateTokenForMultilogin method of the delegate. This should be done if
  // the token was received from this class, but was not accepted by the server
  // (e.g., the server returned 401 Unauthorized).
  virtual void InvalidateTokenForMultilogin(const CoreAccountId& failed_account,
                                            const std::string& token);

  void set_max_authorization_token_fetch_retries_for_testing(int max_retries);
  // Returns the current number of pending fetchers matching given params.
  size_t GetNumPendingRequestsForTesting(const std::string& client_id,
                                         const CoreAccountId& account_id,
                                         const ScopeSet& scopes) const;

  OAuth2TokenServiceDelegate* GetDelegate();
  const OAuth2TokenServiceDelegate* GetDelegate() const;

  // TODO(https://crbug.com/967598): Remove this. It's opened only for
  // OAuth2TokenServiceTest.
  OAuth2TokenService::TokenCache& token_cache();

 protected:
  // Implements a cancelable |OAuth2TokenService::Request|, which should be
  // operated on the UI thread.
  // TODO(davidroche): move this out of header file.
  class RequestImpl : public base::SupportsWeakPtr<RequestImpl>,
                      public Request {
   public:
    // |consumer| is required to outlive this.
    RequestImpl(const std::string& account_id, Consumer* consumer);
    ~RequestImpl() override;

    // Overridden from Request:
    std::string GetAccountId() const override;

    std::string GetConsumerId() const;

    // Informs |consumer_| that this request is completed.
    void InformConsumer(
        const GoogleServiceAuthError& error,
        const OAuth2AccessTokenConsumer::TokenResponse& token_response);

   private:
    // |consumer_| to call back when this request completes.
    const std::string account_id_;
    Consumer* const consumer_;

    SEQUENCE_CHECKER(sequence_checker_);
  };

  // OAuth2TokenServiceObserver:
  void OnRefreshTokensLoaded() override;

  // Implement it in delegates if they want to report errors to the user.
  void UpdateAuthError(const CoreAccountId& account_id,
                       const GoogleServiceAuthError& error);

  // Add a new entry to the cache.
  // Subclasses can override if there are implementation-specific reasons
  // that an access token should ever not be cached.
  virtual void RegisterTokenResponse(
      const std::string& client_id,
      const CoreAccountId& account_id,
      const ScopeSet& scopes,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  // Clears the internal token cache.
  void ClearCache();

  // Clears all of the tokens belonging to |account_id| from the internal token
  // cache. It does not matter what other parameters, like |client_id| were
  // used to request the tokens.
  void ClearCacheForAccount(const CoreAccountId& account_id);

  // Cancels all requests that are currently in progress. Virtual so it can be
  // overridden for tests.
  virtual void CancelAllRequests();

  // Cancels all requests related to a given |account_id|. Virtual so it can be
  // overridden for tests.
  virtual void CancelRequestsForAccount(const CoreAccountId& account_id);

  // Fetches an OAuth token for the specified client/scopes. Virtual so it can
  // be overridden for tests and for platform-specific behavior.
  virtual void FetchOAuth2Token(
      RequestImpl* request,
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const ScopeSet& scopes);

  // Create an access token fetcher for the given account id.
  OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer);

  // Invalidates the |access_token| issued for |account_id|, |client_id| and
  // |scopes|. Virtual so it can be overriden for tests and for platform-
  // specifc behavior.
  virtual void InvalidateAccessTokenImpl(const CoreAccountId& account_id,
                                         const std::string& client_id,
                                         const ScopeSet& scopes,
                                         const std::string& access_token);

  const base::ObserverList<DiagnosticsObserver, true>::Unchecked&
  GetDiagnicsObservers() {
    return diagnostics_observer_list_;
  }

 private:
  class Fetcher;
  friend class Fetcher;
  friend class OAuth2TokenServiceDelegate;

  // Provide a URLLoaderFactory used for fetching access tokens with the
  // |StartRequest| method.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() const;

  // This method does the same as |StartRequestWithContext| except it
  // uses |client_id| and |client_secret| to identify OAuth
  // client app instead of using Chrome's default values.
  std::unique_ptr<Request> StartRequestForClientWithContext(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const ScopeSet& scopes,
      Consumer* consumer);

  // Posts a task to fire the Consumer callback with the cached token response.
  void InformConsumerWithCachedTokenResponse(
      const OAuth2AccessTokenConsumer::TokenResponse* token_response,
      RequestImpl* request,
      const RequestParameters& client_scopes);

  // Returns a currently valid OAuth2 access token for the given set of scopes,
  // or NULL if none have been cached. Note the user of this method should
  // ensure no entry with the same |client_scopes| is added before the usage of
  // the returned entry is done.
  const OAuth2AccessTokenConsumer::TokenResponse* GetCachedTokenResponse(
      const RequestParameters& client_scopes);

  // Removes an access token for the given set of scopes from the cache.
  // Returns true if the entry was removed, otherwise false.
  bool RemoveCachedTokenResponse(const RequestParameters& client_scopes,
                                 const std::string& token_to_remove);

  // Called when |fetcher| finishes fetching.
  void OnFetchComplete(Fetcher* fetcher);

  // Called when a number of fetchers need to be canceled.
  void CancelFetchers(std::vector<Fetcher*> fetchers_to_cancel);

  std::unique_ptr<OAuth2TokenServiceDelegate> delegate_;

  // A map from fetch parameters to a fetcher that is fetching an OAuth2 access
  // token using these parameters.
  std::map<RequestParameters, std::unique_ptr<Fetcher>> pending_fetchers_;

  // List of observers to notify when access token status changes.
  base::ObserverList<DiagnosticsObserver, true>::Unchecked
      diagnostics_observer_list_;

  // The depth of batch changes.
  int batch_change_depth_;

  // Whether all credentials have been loaded.
  bool all_credentials_loaded_;

  std::unique_ptr<OAuth2AccessTokenManager> token_manager_;

  // Maximum number of retries in fetching an OAuth2 access token.
  static int max_fetch_retry_num_;

  FRIEND_TEST_ALL_PREFIXES(OAuth2TokenServiceTest, RequestParametersOrderTest);
  FRIEND_TEST_ALL_PREFIXES(OAuth2TokenServiceTest,
                           SameScopesRequestedForDifferentClients);
  FRIEND_TEST_ALL_PREFIXES(OAuth2TokenServiceTest, UpdateClearsCache);

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenService);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_H_
