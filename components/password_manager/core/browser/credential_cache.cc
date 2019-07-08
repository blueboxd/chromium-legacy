// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_cache.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {

CredentialCache::CredentialCache() = default;
CredentialCache::~CredentialCache() = default;

void CredentialCache::SaveCredentialsForOrigin(
    const std::map<base::string16, const autofill::PasswordForm*>& best_matches,
    const url::Origin& origin) {
  std::vector<CredentialPair> credentials;
  std::transform(best_matches.begin(), best_matches.end(),
                 std::back_inserter(credentials), [](const auto& pair) {
                   return CredentialPair(pair.second->username_value,
                                         pair.second->password_value,
                                         pair.second->origin,
                                         pair.second->is_public_suffix_match);
                 });
  // Sort by origin (but keep the existing username order).
  std::stable_sort(credentials.begin(), credentials.end(),
                   [](const CredentialPair& lhs, const CredentialPair& rhs) {
                     return lhs.origin_url < rhs.origin_url;
                   });
  // Move credentials with exactly matching origins to the top.
  std::stable_partition(credentials.begin(), credentials.end(),
                        [o = origin.GetURL()](const CredentialPair& pair) {
                          return pair.origin_url == o;
                        });
  GetOrCreateCredentialStore(origin)->SaveCredentials(std::move(credentials));
}

const OriginCredentialStore* CredentialCache::GetCredentialStore(
    const url::Origin& origin) {
  return GetOrCreateCredentialStore(origin);
}

void CredentialCache::ClearCredentials() {
  origin_credentials_.clear();
}

OriginCredentialStore* CredentialCache::GetOrCreateCredentialStore(
    const url::Origin& origin) {
  auto it = origin_credentials_.find(origin);
  if (it != origin_credentials_.end())
    return it->second.get();
  auto iter =
      origin_credentials_
          .insert({origin, std::make_unique<OriginCredentialStore>(origin)})
          .first;
  return iter->second.get();
}
}  // namespace password_manager
