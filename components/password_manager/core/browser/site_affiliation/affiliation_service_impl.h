// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_AFFILIATION_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_AFFILIATION_SERVICE_IMPL_H_

#include <map>

#include "components/password_manager/core/browser/site_affiliation/affiliation_service.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace syncer {
class SyncService;
}

namespace password_manager {

class AffiliationServiceImpl : public password_manager::AffiliationService {
 public:
  explicit AffiliationServiceImpl(syncer::SyncService* sync_service);
  ~AffiliationServiceImpl() override;

  // Prefetches change password URLs and saves them to changePasswordUrls map.
  // The verification if a user is synced and does not use a passphrase must be
  // performed.
  void PrefetchChangePasswordURLs(
      const std::vector<url::Origin>& origins) override;

  // Clears the map of URL and cancels prefetch if still running.
  void Clear() override;

  // Returns a URL with change password form for a site requested.
  // In case no valid URL was found, the entry in map for |origin| still exists
  // and a method returns an empty URL.
  GURL GetChangePasswordURL(const url::Origin& origin) override;

 private:
  syncer::SyncService* sync_service_;
  std::map<url::Origin, GURL> change_password_urls_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_AFFILIATION_SERVICE_IMPL_H_
