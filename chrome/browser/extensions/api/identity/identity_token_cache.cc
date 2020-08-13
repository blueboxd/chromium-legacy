// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_token_cache.h"

#include <algorithm>

#include "base/stl_util.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"

namespace extensions {

IdentityTokenCacheValue::IdentityTokenCacheValue() = default;
IdentityTokenCacheValue::IdentityTokenCacheValue(
    const IdentityTokenCacheValue& other) = default;
IdentityTokenCacheValue& IdentityTokenCacheValue::operator=(
    const IdentityTokenCacheValue& other) = default;
IdentityTokenCacheValue::~IdentityTokenCacheValue() = default;

// static
IdentityTokenCacheValue IdentityTokenCacheValue::CreateIssueAdvice(
    const IssueAdviceInfo& issue_advice) {
  IdentityTokenCacheValue cache_value;
  cache_value.status_ = CACHE_STATUS_ADVICE;
  cache_value.issue_advice_ = issue_advice;
  cache_value.expiration_time_ =
      base::Time::Now() + base::TimeDelta::FromSeconds(
                              identity_constants::kCachedIssueAdviceTTLSeconds);
  return cache_value;
}

// static
IdentityTokenCacheValue IdentityTokenCacheValue::CreateRemoteConsent(
    const RemoteConsentResolutionData& resolution_data) {
  IdentityTokenCacheValue cache_value;
  cache_value.status_ = CACHE_STATUS_REMOTE_CONSENT;
  cache_value.resolution_data_ = resolution_data;
  cache_value.expiration_time_ =
      base::Time::Now() + base::TimeDelta::FromSeconds(
                              identity_constants::kCachedIssueAdviceTTLSeconds);
  return cache_value;
}

// static
IdentityTokenCacheValue IdentityTokenCacheValue::CreateRemoteConsentApproved(
    const std::string& consent_result) {
  IdentityTokenCacheValue cache_value;
  cache_value.status_ = CACHE_STATUS_REMOTE_CONSENT_APPROVED;
  cache_value.consent_result_ = consent_result;
  cache_value.expiration_time_ =
      base::Time::Now() + base::TimeDelta::FromSeconds(
                              identity_constants::kCachedIssueAdviceTTLSeconds);
  return cache_value;
}

// static
IdentityTokenCacheValue IdentityTokenCacheValue::CreateToken(
    const std::string& token,
    base::TimeDelta time_to_live) {
  IdentityTokenCacheValue cache_value;
  cache_value.status_ = CACHE_STATUS_TOKEN;
  cache_value.token_ = token;

  // Remove 20 minutes from the ttl so cached tokens will have some time
  // to live any time they are returned.
  time_to_live -= base::TimeDelta::FromMinutes(20);

  base::TimeDelta zero_delta;
  if (time_to_live < zero_delta)
    time_to_live = zero_delta;

  cache_value.expiration_time_ = base::Time::Now() + time_to_live;
  return cache_value;
}

IdentityTokenCacheValue::CacheValueStatus IdentityTokenCacheValue::status()
    const {
  if (is_expired())
    return IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND;
  else
    return status_;
}

bool IdentityTokenCacheValue::is_expired() const {
  return status_ == CACHE_STATUS_NOTFOUND ||
         expiration_time_ < base::Time::Now();
}

const base::Time& IdentityTokenCacheValue::expiration_time() const {
  return expiration_time_;
}

const IssueAdviceInfo& IdentityTokenCacheValue::issue_advice() const {
  return issue_advice_;
}

const RemoteConsentResolutionData& IdentityTokenCacheValue::resolution_data()
    const {
  return resolution_data_;
}

const std::string& IdentityTokenCacheValue::consent_result() const {
  return consent_result_;
}

const std::string& IdentityTokenCacheValue::token() const {
  return token_;
}

IdentityTokenCache::IdentityTokenCache() = default;
IdentityTokenCache::~IdentityTokenCache() = default;

void IdentityTokenCache::SetToken(const ExtensionTokenKey& key,
                                  const IdentityTokenCacheValue& token_data) {
  auto it = token_cache_.find(key);
  if (it != token_cache_.end() && it->second.status() <= token_data.status())
    token_cache_.erase(it);

  token_cache_.insert(std::make_pair(key, token_data));
}

void IdentityTokenCache::EraseToken(const std::string& extension_id,
                                    const std::string& token) {
  CachedTokens::iterator it;
  for (it = token_cache_.begin(); it != token_cache_.end(); ++it) {
    if (it->first.extension_id == extension_id &&
        it->second.status() == IdentityTokenCacheValue::CACHE_STATUS_TOKEN &&
        it->second.token() == token) {
      token_cache_.erase(it);
      break;
    }
  }
}

void IdentityTokenCache::EraseAllTokens() {
  token_cache_.clear();
}

const IdentityTokenCacheValue& IdentityTokenCache::GetToken(
    const ExtensionTokenKey& key) {
  return token_cache_[key];
}

const IdentityTokenCache::CachedTokens& IdentityTokenCache::GetAllTokens() {
  return token_cache_;
}

}  // namespace extensions
