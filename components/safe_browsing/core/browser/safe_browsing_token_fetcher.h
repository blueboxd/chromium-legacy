// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCHER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
class IdentityManager;
class AccessTokenFetcher;
}  // namespace signin

namespace safe_browsing {

// This class is used to fetch access tokens for communcations with Safe
// Browsing. It asynchronously returns the access token for the current
// primary account, or nullopt if an error occurred. This must be
// run on the UI thread.
class SafeBrowsingTokenFetcher {
 public:
  using Callback =
      base::OnceCallback<void(base::Optional<signin::AccessTokenInfo>)>;

  // Create a SafeBrowsingTokenFetcher and immediately begin fetching the token
  // for the primary account of |identity_manager|, with consent level
  // |consent|. |callback| will be called with the result.
  SafeBrowsingTokenFetcher(signin::IdentityManager* identity_manager,
                           signin::ConsentLevel consent,
                           Callback callback);

  ~SafeBrowsingTokenFetcher();

 private:
  void OnTokenFetched(GoogleServiceAuthError error,
                      signin::AccessTokenInfo access_token_info);
  void OnTokenTimeout();
  void Finish(base::Optional<signin::AccessTokenInfo> token_info);

  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher_;
  Callback callback_;

  base::WeakPtrFactory<SafeBrowsingTokenFetcher> weak_ptr_factory_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCHER_H_
