// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/same_site_data_remover_impl.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

void OnGetAllCookies(base::OnceClosure closure,
                     network::mojom::CookieManager* cookie_manager,
                     std::set<std::string>* same_site_none_domains,
                     const std::vector<net::CanonicalCookie>& cookies) {
  base::RepeatingClosure barrier =
      base::BarrierClosure(cookies.size(), std::move(closure));
  for (const auto& cookie : cookies) {
    if (cookie.IsEffectivelySameSiteNone()) {
      same_site_none_domains->emplace(cookie.Domain());
      cookie_manager->DeleteCanonicalCookie(
          cookie, base::BindOnce([](const base::RepeatingClosure& callback,
                                    bool success) { callback.Run(); },
                                 barrier));
    } else {
      barrier.Run();
    }
  }
}

bool DoesOriginMatchDomain(const std::set<std::string>& same_site_none_domains,
                           const url::Origin& origin,
                           storage::SpecialStoragePolicy* policy) {
  for (const std::string& domain : same_site_none_domains) {
    if (net::cookie_util::IsDomainMatch(domain, origin.host())) {
      return true;
    }
  }
  return false;
}

}  // namespace

SameSiteDataRemoverImpl::SameSiteDataRemoverImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      storage_partition_(
          BrowserContext::GetDefaultStoragePartition(browser_context_)) {
  DCHECK(browser_context_);
}

SameSiteDataRemoverImpl::~SameSiteDataRemoverImpl() {}

const std::set<std::string>&
SameSiteDataRemoverImpl::GetDeletedDomainsForTesting() {
  return same_site_none_domains_;
}

void SameSiteDataRemoverImpl::OverrideStoragePartitionForTesting(
    StoragePartition* storage_partition) {
  storage_partition_ = storage_partition;
}

void SameSiteDataRemoverImpl::DeleteSameSiteNoneCookies(
    base::OnceClosure closure) {
  same_site_none_domains_.clear();
  auto* cookie_manager =
      storage_partition_->GetCookieManagerForBrowserProcess();
  cookie_manager->GetAllCookies(
      base::BindOnce(&OnGetAllCookies, std::move(closure), cookie_manager,
                     &same_site_none_domains_));
}

void SameSiteDataRemoverImpl::ClearStoragePartitionData(
    base::OnceClosure closure) {
  const uint32_t storage_partition_removal_mask =
      content::StoragePartition::REMOVE_DATA_MASK_ALL &
      ~content::StoragePartition::REMOVE_DATA_MASK_COOKIES;
  const uint32_t quota_storage_removal_mask =
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL;
  // TODO(crbug.com/987177): Figure out how to handle protected storage.

  storage_partition_->ClearData(
      storage_partition_removal_mask, quota_storage_removal_mask,
      base::BindRepeating(&DoesOriginMatchDomain, same_site_none_domains_),
      nullptr, false, base::Time(), base::Time::Max(), std::move(closure));
}

}  // namespace content
