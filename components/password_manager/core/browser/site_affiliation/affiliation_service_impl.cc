// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/site_affiliation/affiliation_service_impl.h"

#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Checks if a user is synced.
bool IsUserSynced(syncer::SyncService* sync_service) {
  return sync_service->IsSyncFeatureEnabled();
}

// Checks if a user has a custom passphrase set.
bool IsPassphraseSet(syncer::SyncService* sync_service) {
  return sync_service->GetUserSettings()->IsPassphraseRequired();
}

}  // namespace

namespace password_manager {

AffiliationServiceImpl::AffiliationServiceImpl(
    syncer::SyncService* sync_service)
    : sync_service_(sync_service) {}

AffiliationServiceImpl::~AffiliationServiceImpl() = default;

void AffiliationServiceImpl::PrefetchChangePasswordURLs(
    const std::vector<url::Origin>& origins) {
  if (IsUserSynced(sync_service_) && !IsPassphraseSet(sync_service_)) {
    // AffiliationFetcher::Create and AffiliationFetcher::StartRequest
  }
}

void AffiliationServiceImpl::Clear() {
  change_password_urls_.clear();
}

GURL AffiliationServiceImpl::GetChangePasswordURL(const url::Origin& origin) {
  auto it = change_password_urls_.find(origin);
  return it != change_password_urls_.end() ? it->second : GURL();
}

}  // namespace password_manager
