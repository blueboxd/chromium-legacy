// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_DIAGNOSTICS_OBSERVER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_DIAGNOSTICS_OBSERVER_H_

#include "base/time/time.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

// TODO(https://crbug.com/967598): Move this class to OAuth2AccessTokenManager
// and discard this file once it's not referred by OAuth2TokenService.

// Classes that want to monitor status of access token and access token
// request should implement this interface and register with the
// AddOAccessTokenDiagnosticsObserver() call.
class AccessTokenDiagnosticsObserver {
 public:
  // A set of scopes in OAuth2 authentication.
  typedef std::set<std::string> ScopeSet;

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
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_DIAGNOSTICS_OBSERVER_H_
