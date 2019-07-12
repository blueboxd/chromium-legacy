// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "build/buildflag.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "net/base/backoff_entry.h"

namespace identity {
class IdentityManager;
}

class PrefService;
class PrefRegistrySimple;

// ProfileOAuth2TokenService is a KeyedService that retrieves
// OAuth2 access tokens for a given set of scopes using the OAuth2 login
// refresh tokens.
//
// See |OAuth2TokenService| for usage details.
//
// Note: after StartRequest returns, in-flight requests will continue
// even if the TokenService refresh token that was used to initiate
// the request changes or is cleared.  When the request completes,
// Consumer::OnGetTokenSuccess will be invoked, but the access token
// won't be cached.
//
// Note: requests should be started from the UI thread.
class ProfileOAuth2TokenService : public OAuth2TokenService {
 public:
  typedef base::RepeatingCallback<void(const CoreAccountId& /* account_id */,
                                       bool /* is_refresh_token_valid */,
                                       const std::string& /* source */)>
      RefreshTokenAvailableFromSourceCallback;
  typedef base::RepeatingCallback<void(const CoreAccountId& /* account_id */,
                                       const std::string& /* source */)>
      RefreshTokenRevokedFromSourceCallback;

  ProfileOAuth2TokenService(
      PrefService* user_prefs,
      std::unique_ptr<OAuth2TokenServiceDelegate> delegate);
  ~ProfileOAuth2TokenService() override;

  // Registers per-profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Checks in the cache for a valid access token for a specified |account_id|
  // and |scopes|, and if not found starts a request for an OAuth2 access token
  // using the OAuth2 refresh token maintained by this instance for that
  // |account_id|. The caller owns the returned Request.
  // |scopes| is the set of scopes to get an access token for, |consumer| is
  // the object that will be called back with results if the returned request
  // is not deleted.
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartRequest(
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
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartRequestForClient(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      OAuth2AccessTokenManager::Consumer* consumer);

  // This method does the same as |StartRequest| except it uses the
  // URLLoaderFactory given by |url_loader_factory| instead of using the one
  // returned by Delegate::GetURLLoaderFactory().
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartRequestWithContext(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      OAuth2AccessTokenManager::Consumer* consumer);

  // Mark an OAuth2 |access_token| issued for |account_id| and |scopes| as
  // invalid. This should be done if the token was received from this class,
  // but was not accepted by the server (e.g., the server returned
  // 401 Unauthorized). The token will be removed from the cache for the given
  // scopes.
  void InvalidateAccessToken(const CoreAccountId& account_id,
                             const OAuth2AccessTokenManager::ScopeSet& scopes,
                             const std::string& access_token);

  // Removes token from cache (if it is cached) and calls
  // InvalidateTokenForMultilogin method of the delegate. This should be done if
  // the token was received from this class, but was not accepted by the server
  // (e.g., the server returned 401 Unauthorized).
  virtual void InvalidateTokenForMultilogin(const CoreAccountId& failed_account,
                                            const std::string& token);

  // If set, this callback will be invoked when a new refresh token is
  // available. Contains diagnostic information about the source of the update
  // credentials operation.
  void SetRefreshTokenAvailableFromSourceCallback(
      RefreshTokenAvailableFromSourceCallback callback);

  // If set, this callback will be invoked when a refresh token is revoked.
  // Contains diagnostic information about the source that initiated the
  // revocation operation.
  void SetRefreshTokenRevokedFromSourceCallback(
      RefreshTokenRevokedFromSourceCallback callback);

  void Shutdown();

  // Loads credentials from a backing persistent store to make them available
  // after service is used between profile restarts.
  //
  // The primary account is specified with the |primary_account_id| argument.
  // For a regular profile, the primary account id comes from
  // PrimaryAccountManager.
  // For a supervised user, the id comes from SupervisedUserService.
  void LoadCredentials(const CoreAccountId& primary_account_id);

  // Returns true if LoadCredentials finished with no errors.
  bool HasLoadCredentialsFinishedWithNoErrors();

  // Updates a |refresh_token| for an |account_id|. Credentials are persisted,
  // and available through |LoadCredentials| after service is restarted.
  void UpdateCredentials(
      const CoreAccountId& account_id,
      const std::string& refresh_token,
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  void RevokeCredentials(
      const CoreAccountId& account_id,
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Revokes all credentials.
  void RevokeAllCredentials(
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Returns a pointer to its instance of net::BackoffEntry or nullptr if there
  // is no such instance.
  const net::BackoffEntry* GetDelegateBackoffEntry();

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Removes the credentials associated to account_id from the internal storage,
  // and moves them to |to_service|. The credentials are not revoked on the
  // server, but the OnRefreshTokenRevoked() notification is sent to the
  // observers.
  void ExtractCredentials(ProfileOAuth2TokenService* to_service,
                          const CoreAccountId& account_id);
#endif

  // Exposes the ability to update auth errors to tests.
  void UpdateAuthErrorForTesting(const CoreAccountId& account_id,
                                 const GoogleServiceAuthError& error);

 private:
  friend class identity::IdentityManager;

  // OAuth2TokenServiceObserver implementation.
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override;
  void OnRefreshTokenRevoked(const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;

  // Clears the internal token cache.
  void ClearCache();

  // Clears all of the tokens belonging to |account_id| from the internal token
  // cache. It does not matter what other parameters, like |client_id| were
  // used to request the tokens.
  void ClearCacheForAccount(const CoreAccountId& account_id);

  // Creates a new device ID if there are no accounts, or if the current device
  // ID is empty.
  void RecreateDeviceIdIfNeeded();

  PrefService* user_prefs_;

  // Callbacks to invoke, if set, for refresh token-related events.
  RefreshTokenAvailableFromSourceCallback on_refresh_token_available_callback_;
  RefreshTokenRevokedFromSourceCallback on_refresh_token_revoked_callback_;

  signin_metrics::SourceForRefreshTokenOperation update_refresh_token_source_ =
      signin_metrics::SourceForRefreshTokenOperation::kUnknown;

  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceTest, UpdateClearsCache);

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_
