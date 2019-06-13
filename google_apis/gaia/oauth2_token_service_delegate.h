// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
#define GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_DELEGATE_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/backoff_entry.h"

namespace network {
class SharedURLLoaderFactory;
}

// Abstract base class to fetch and maintain refresh tokens from various
// entities. Concrete subclasses should implement RefreshTokenIsAvailable and
// CreateAccessTokenFetcher properly.
class OAuth2TokenServiceDelegate {
 public:
  // Refresh token guaranteed to be invalid. Can be passed to
  // UpdateCredentials() to force an authentication error.
  static const char kInvalidRefreshToken[];

  enum LoadCredentialsState {
    LOAD_CREDENTIALS_NOT_STARTED,
    LOAD_CREDENTIALS_IN_PROGRESS,
    LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
    LOAD_CREDENTIALS_FINISHED_WITH_DB_ERRORS,
    LOAD_CREDENTIALS_FINISHED_WITH_DECRYPT_ERRORS,
    LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT,
    LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS,
  };

  OAuth2TokenServiceDelegate();
  virtual ~OAuth2TokenServiceDelegate();

  virtual OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) = 0;

  // Returns |true| if a refresh token is available for |account_id|, and
  // |false| otherwise.
  // Note: Implementations must make sure that |RefreshTokenIsAvailable| returns
  // |true| if and only if |account_id| is contained in the list of accounts
  // returned by |GetAccounts|.
  virtual bool RefreshTokenIsAvailable(
      const CoreAccountId& account_id) const = 0;
  virtual GoogleServiceAuthError GetAuthError(
      const CoreAccountId& account_id) const;
  virtual void UpdateAuthError(const CoreAccountId& account_id,
                               const GoogleServiceAuthError& error) {}

  // Returns a list of accounts for which a refresh token is maintained by
  // |this| instance.
  // Note: If tokens have not been fully loaded yet, an empty list is returned.
  // Also, see |RefreshTokenIsAvailable|.
  virtual std::vector<std::string> GetAccounts();
  virtual void RevokeAllCredentials() {}

  virtual void InvalidateAccessToken(const CoreAccountId& account_id,
                                     const std::string& client_id,
                                     const std::set<std::string>& scopes,
                                     const std::string& access_token) {}

  // If refresh token is accessible (on Desktop) sets error for it to
  // INVALID_GAIA_CREDENTIALS and notifies the observers. Otherwise
  // does nothing.
  virtual void InvalidateTokenForMultilogin(
      const CoreAccountId& failed_account) {}

  virtual void Shutdown() {}
  virtual void UpdateCredentials(const CoreAccountId& account_id,
                                 const std::string& refresh_token) {}
  virtual void RevokeCredentials(const CoreAccountId& account_id) {}
  virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const;

  // Returns refresh token if the platform allows it (on Desktop) and if it is
  // available and doesn't have error. Otherwise returns empty string (for iOS
  // and Android).
  virtual std::string GetTokenForMultilogin(
      const CoreAccountId& account_id) const;

  bool ValidateAccountId(const CoreAccountId& account_id) const;

  // Add or remove observers of this token service.
  void AddObserver(OAuth2TokenServiceObserver* observer);
  void RemoveObserver(OAuth2TokenServiceObserver* observer);

  // Returns a pointer to its instance of net::BackoffEntry if it has one, or
  // a nullptr otherwise.
  virtual const net::BackoffEntry* BackoffEntry() const;

  // -----------------------------------------------------------------------
  // Methods that are only used by ProfileOAuth2TokenService.
  // -----------------------------------------------------------------------

  // Loads the credentials from disk. Called only once when the token service
  // is initialized. Default implementation is NOTREACHED - subsclasses that
  // are used by the ProfileOAuth2TokenService must provide an implementation
  // for this method.
  virtual void LoadCredentials(const CoreAccountId& primary_account_id);

  // Returns the state of the load credentials operation.
  LoadCredentialsState load_credentials_state() const {
    return load_credentials_state_;
  }

  // Removes the credentials associated to account_id from the internal storage,
  // and moves them to |to_service|. The credentials are not revoked on the
  // server, but the OnRefreshTokenRevoked() notification is sent to the
  // observers.
  virtual void ExtractCredentials(OAuth2TokenService* to_service,
                                  const CoreAccountId& account_id);

  // Attempts to fix the error if possible.  Returns true if the error was fixed
  // and false otherwise.
  virtual bool FixRequestErrorIfPossible();

#if defined(OS_IOS)
  // Triggers platform specific implementation for IOS to add a given account
  // to the token service from a system account.
  virtual void AddAccountFromSystem(const CoreAccountId& account_id) {}
#endif

#if defined(OS_ANDROID) || defined(OS_IOS)
  // Triggers platform specific implementation for Android and IOS to reload
  // accounts from system.
  virtual void ReloadAccountsFromSystem(
      const CoreAccountId& primary_account_id) {}
#endif

  // -----------------------------------------------------------------------
  // End of methods that are only used by ProfileOAuth2TokenService
  // -----------------------------------------------------------------------

 protected:
  void set_load_credentials_state(LoadCredentialsState state) {
    load_credentials_state_ = state;
  }

  // Called by subclasses to notify observers.
  void FireRefreshTokenAvailable(const CoreAccountId& account_id);
  void FireRefreshTokenRevoked(const CoreAccountId& account_id);
  // FireRefreshTokensLoaded is virtual and overridden in android implementation
  // where additional actions are required.
  virtual void FireRefreshTokensLoaded();
  void FireAuthErrorChanged(const CoreAccountId& account_id,
                            const GoogleServiceAuthError& error);

  // Helper class to scope batch changes.
  class ScopedBatchChange {
   public:
    explicit ScopedBatchChange(OAuth2TokenServiceDelegate* delegate);
    ~ScopedBatchChange();

   private:
    OAuth2TokenServiceDelegate* delegate_;  // Weak.
    DISALLOW_COPY_AND_ASSIGN(ScopedBatchChange);
  };

 private:
  // List of observers to notify when refresh token availability changes.
  // Makes sure list is empty on destruction.
  base::ObserverList<OAuth2TokenServiceObserver, true>::Unchecked
      observer_list_;

  // The state of the load credentials operation.
  LoadCredentialsState load_credentials_state_ = LOAD_CREDENTIALS_NOT_STARTED;

  void StartBatchChanges();
  void EndBatchChanges();

  // The depth of batch changes.
  int batch_change_depth_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenServiceDelegate);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
