// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "build/buildflag.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "net/base/backoff_entry.h"

#include <memory>

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
// Note: requests should be started from the UI thread. To start a
// request from other thread, please use OAuth2TokenServiceRequest.
class ProfileOAuth2TokenService : public OAuth2TokenService {
 public:
  ProfileOAuth2TokenService(
      PrefService* user_prefs,
      std::unique_ptr<OAuth2TokenServiceDelegate> delegate);
  ~ProfileOAuth2TokenService() override;

  // Registers per-profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

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
                                 const GoogleServiceAuthError& error) {
    UpdateAuthError(account_id, error);
  }

 private:
  friend class identity::IdentityManager;

  // OAuth2TokenServiceObserver implementation.
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override;
  void OnRefreshTokenRevoked(const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;

  // Creates a new device ID if there are no accounts, or if the current device
  // ID is empty.
  void RecreateDeviceIdIfNeeded();

  PrefService* user_prefs_;

  signin_metrics::SourceForRefreshTokenOperation update_refresh_token_source_ =
      signin_metrics::SourceForRefreshTokenOperation::kUnknown;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_
