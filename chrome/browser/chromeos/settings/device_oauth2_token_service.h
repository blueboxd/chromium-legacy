// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_delegate.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "google_apis/gaia/oauth2_token_service_observer.h"

class PrefRegistrySimple;

namespace chromeos {

// DeviceOAuth2TokenService retrieves OAuth2 access tokens for a given
// set of scopes using the device-level OAuth2 any-api refresh token
// obtained during enterprise device enrollment.
// When using DeviceOAuth2TokenService, a value of |GetRobotAccountId| should
// be used in places where API expects |account_id|.
//
// Note that requests must be made from the UI thread.
class DeviceOAuth2TokenService
    : public OAuth2TokenServiceObserver,
      public OAuth2AccessTokenManager::Delegate,
      public DeviceOAuth2TokenServiceDelegate::ValidationStatusDelegate {
 public:
  typedef base::RepeatingCallback<void(const CoreAccountId& /* account_id */)>
      RefreshTokenAvailableCallback;
  typedef base::RepeatingCallback<void(const CoreAccountId& /* account_id */)>
      RefreshTokenRevokedCallback;

  typedef base::Callback<void(bool)> StatusCallback;

  // Persist the given refresh token on the device. Overwrites any previous
  // value. Should only be called during initial device setup. Signals
  // completion via the given callback, passing true if the operation succeeded.
  void SetAndSaveRefreshToken(const std::string& refresh_token,
                              const StatusCallback& callback);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Pull the robot account ID from device policy.
  virtual std::string GetRobotAccountId() const;

  // Can be used to override the robot account ID for testing purposes. Most
  // common use case is to easily inject a non-empty account ID to make the
  // refresh token for the robot account visible via GetAccounts() and
  // RefreshTokenIsAvailable().
  void set_robot_account_id_for_testing(const CoreAccountId& account_id);

  // If set, this callback will be invoked when a new refresh token is
  // available.
  void SetRefreshTokenAvailableCallback(RefreshTokenAvailableCallback callback);

  // If set, this callback will be invoked when a refresh token is revoked.
  void SetRefreshTokenRevokedCallback(RefreshTokenRevokedCallback callback);

  // Checks in the cache for a valid access token for a specified |account_id|
  // and |scopes|, and if not found starts a request for an OAuth2 access token
  // using the OAuth2 refresh token maintained by this instance for that
  // |account_id|. The caller owns the returned Request.
  // |scopes| is the set of scopes to get an access token for, |consumer| is
  // the object that will be called back with results if the returned request
  // is not deleted.
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartAccessTokenRequest(
      const CoreAccountId& account_id,
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

  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const;

  OAuth2AccessTokenManager* GetAccessTokenManager();

 private:
  friend class DeviceOAuth2TokenServiceFactory;
  friend class DeviceOAuth2TokenServiceTest;
  struct PendingRequest;

  // OAuth2AccessTokenManager::Delegate:
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) override;
  bool HasRefreshToken(const CoreAccountId& account_id) const override;
  bool FixRequestErrorIfPossible() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;
  bool HandleAccessTokenFetch(
      OAuth2AccessTokenManager::RequestImpl* request,
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2AccessTokenManager::ScopeSet& scopes) override;
  void OnAccessTokenInvalidated(const CoreAccountId& account_id,
                                const std::string& client_id,
                                const std::set<std::string>& scopes,
                                const std::string& access_token) override;
  void OnAccessTokenFetched(const CoreAccountId& account_id,
                            const GoogleServiceAuthError& error) override;

  // OAuth2TokenServiceObserver:
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override;
  void OnRefreshTokenRevoked(const CoreAccountId& account_id) override;

  // Implementation of
  // DeviceOAuth2TokenServiceDelegate::ValidationStatusDelegate.
  void OnValidationCompleted(GoogleServiceAuthError::State error) override;

  // Use DeviceOAuth2TokenServiceFactory to get an instance of this class.
  explicit DeviceOAuth2TokenService(
      std::unique_ptr<DeviceOAuth2TokenServiceDelegate> delegate);
  ~DeviceOAuth2TokenService() override;

  // Flushes |pending_requests_|, indicating the specified result.
  void FlushPendingRequests(bool token_is_valid,
                            GoogleServiceAuthError::State error);

  // Signals failure on the specified request, passing |error| as the reason.
  void FailRequest(OAuth2AccessTokenManager::RequestImpl* request,
                   GoogleServiceAuthError::State error);

  // TODO(https://crbug.com/967598): Merge DeviceOAuth2TokenServiceDelegate
  // into DeviceOAuth2TokenService.
  std::unique_ptr<DeviceOAuth2TokenServiceDelegate> delegate_;
  std::unique_ptr<OAuth2AccessTokenManager> token_manager_;

  // Currently open requests that are waiting while loading the system salt or
  // validating the token.
  std::vector<PendingRequest*> pending_requests_;

  // Callbacks to invoke, if set, for refresh token-related events.
  RefreshTokenAvailableCallback on_refresh_token_available_callback_;
  RefreshTokenRevokedCallback on_refresh_token_revoked_callback_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOAuth2TokenService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_
