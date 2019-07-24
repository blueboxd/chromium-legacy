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
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace content {

namespace {

void OnGetAllCookies(base::OnceClosure closure,
                     network::mojom::CookieManager* cookie_manager,
                     std::set<std::string>* same_site_none_domains,
                     const std::vector<net::CanonicalCookie>& cookies) {
  base::RepeatingClosure barrier =
      base::BarrierClosure(cookies.size(), std::move(closure));
  for (const auto& cookie : cookies) {
    if (cookie.GetEffectiveSameSite() == net::CookieSameSite::NO_RESTRICTION) {
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

}  // namespace

SameSiteDataRemoverImpl::SameSiteDataRemoverImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context_);
}

SameSiteDataRemoverImpl::~SameSiteDataRemoverImpl() {}

const std::set<std::string>&
SameSiteDataRemoverImpl::GetDeletedDomainsForTesting() {
  return same_site_none_domains_;
}

void SameSiteDataRemoverImpl::DeleteSameSiteNoneCookies(
    base::OnceClosure closure) {
  same_site_none_domains_.clear();
  StoragePartition* storage_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context_);
  auto* cookie_manager = storage_partition->GetCookieManagerForBrowserProcess();
  cookie_manager->GetAllCookies(
      base::BindOnce(&OnGetAllCookies, std::move(closure), cookie_manager,
                     &same_site_none_domains_));
}

}  // namespace content
