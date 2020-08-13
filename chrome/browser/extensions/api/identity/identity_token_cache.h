// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_TOKEN_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_TOKEN_CACHE_H_

#include <map>
#include <set>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/extensions/api/identity/extension_token_key.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

namespace extensions {

class IdentityTokenCacheValue {
 public:
  IdentityTokenCacheValue();
  IdentityTokenCacheValue(const IdentityTokenCacheValue& other);
  IdentityTokenCacheValue& operator=(const IdentityTokenCacheValue& other);
  ~IdentityTokenCacheValue();

  static IdentityTokenCacheValue CreateIssueAdvice(
      const IssueAdviceInfo& issue_advice);
  static IdentityTokenCacheValue CreateRemoteConsent(
      const RemoteConsentResolutionData& resolution_data);
  static IdentityTokenCacheValue CreateRemoteConsentApproved(
      const std::string& consent_result);
  static IdentityTokenCacheValue CreateToken(const std::string& token,
                                             base::TimeDelta time_to_live);

  // Order of these entries is used to determine whether or not new
  // entries supersede older ones in SetCachedToken.
  enum CacheValueStatus {
    CACHE_STATUS_NOTFOUND,
    CACHE_STATUS_ADVICE,
    CACHE_STATUS_REMOTE_CONSENT,
    CACHE_STATUS_REMOTE_CONSENT_APPROVED,
    CACHE_STATUS_TOKEN
  };

  CacheValueStatus status() const;
  const base::Time& expiration_time() const;

  const IssueAdviceInfo& issue_advice() const;
  const RemoteConsentResolutionData& resolution_data() const;
  const std::string& consent_result() const;
  const std::string& token() const;

 private:
  bool is_expired() const;

  CacheValueStatus status_ = CACHE_STATUS_NOTFOUND;
  base::Time expiration_time_;

  // TODO(alexilin): This class holds at any given time one of the several
  // possible types. Consider rewriting using absl::variant
  IssueAdviceInfo issue_advice_;
  RemoteConsentResolutionData resolution_data_;
  std::string consent_result_;
  std::string token_;
};

// In-memory cache of OAuth2 access tokens that are requested by extensions
// through the `getAuthToken` API. Also caches intermediate short-lived values
// used at different stages of the `getAuthToken` flow before a token is
// obtained. The cache automatically handles token expiration. Extensions can
// manually remove tokens from the cache using `removeCachedAuthToken` API.
//
// chrome://identity-internals provides a view of cache's content for debugging.
class IdentityTokenCache {
 public:
  IdentityTokenCache();
  ~IdentityTokenCache();
  IdentityTokenCache(const IdentityTokenCache& other) = delete;
  IdentityTokenCache& operator=(const IdentityTokenCache& other) = delete;

  using CachedTokens = std::map<ExtensionTokenKey, IdentityTokenCacheValue>;

  void SetToken(const ExtensionTokenKey& key,
                const IdentityTokenCacheValue& token_data);
  void EraseToken(const std::string& extension_id, const std::string& token);
  void EraseAllTokens();
  const IdentityTokenCacheValue& GetToken(const ExtensionTokenKey& key);
  const CachedTokens& GetAllTokens();

 private:
  CachedTokens token_cache_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_TOKEN_CACHE_H_
