// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/favicon_cache.h"

#include <utility>

#include "base/bind.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync/protocol/session_specifics.pb.h"

namespace sync_sessions {

struct FaviconInfo {
  explicit FaviconInfo(const GURL& favicon_url)
      : favicon_url(favicon_url), last_visit_time(base::Time::Now()) {}

  // The URL this favicon was loaded from.
  const GURL favicon_url;
  // The last time a tab needed this favicon.
  const base::Time last_visit_time;

 private:
  DISALLOW_COPY_AND_ASSIGN(FaviconInfo);
};

namespace {

// Desired size of favicons to load from the cache when translating page url to
// icon url.
const int kDesiredSizeInPx = 16;

// Returns a mask of the supported favicon types.
favicon_base::IconTypeSet SupportedFaviconTypes() {
  return {favicon_base::IconType::kFavicon};
}

}  // namespace

FaviconCache::FaviconCache(favicon::FaviconService* favicon_service,
                           history::HistoryService* history_service,
                           int max_mappings_limit)
    : favicon_service_(favicon_service),
      max_mappings_limit_(max_mappings_limit) {
  if (history_service)
    history_service_observer_.Add(history_service);
  DVLOG(1) << "Setting mapping limit to " << max_mappings_limit;
}

FaviconCache::~FaviconCache() {}

void FaviconCache::OnPageFaviconUpdated(const GURL& page_url) {
  DCHECK(page_url.is_valid());

  // If a favicon load is already happening for this url, let it finish.
  if (page_task_map_.find(page_url) != page_task_map_.end())
    return;

  // If a mapping already exists, rely on the cached mapping.
  const GURL favicon_url = GetIconUrlForPageUrl(page_url);
  if (favicon_url.is_valid()) {
    // Reset the same value to update the last visit time.
    SetIconUrlForPageUrl(page_url, favicon_url);
    return;
  }

  DVLOG(1) << "Triggering favicon load for url " << page_url.spec();

  // Can be nullptr in tests.
  if (!favicon_service_)
    return;

  base::CancelableTaskTracker::TaskId id =
      favicon_service_->GetFaviconForPageURL(
          page_url, SupportedFaviconTypes(), kDesiredSizeInPx,
          base::BindOnce(&FaviconCache::OnFaviconDataAvailable,
                         weak_ptr_factory_.GetWeakPtr(), page_url),
          &cancelable_task_tracker_);
  page_task_map_[page_url] = id;
}

void FaviconCache::OnFaviconVisited(const GURL& page_url,
                                    const GURL& favicon_url) {
  DCHECK(page_url.is_valid());
  if (!favicon_url.is_valid()) {
    OnPageFaviconUpdated(page_url);
    return;
  }

  SetIconUrlForPageUrl(page_url, favicon_url);
}

GURL FaviconCache::GetIconUrlForPageUrl(const GURL& page_url) const {
  auto iter = page_favicon_map_.find(page_url);
  if (iter == page_favicon_map_.end())
    return GURL();
  return iter->second->favicon_url;
}

void FaviconCache::UpdateMappingsFromForeignTab(
    const sync_pb::SessionTab& tab) {
  for (const sync_pb::TabNavigation& navigation : tab.navigation()) {
    const GURL page_url(navigation.virtual_url());
    const GURL icon_url(navigation.favicon_url());

    if (!icon_url.is_valid() || !page_url.is_valid() ||
        icon_url.SchemeIs("data")) {
      continue;
    }

    SetIconUrlForPageUrl(page_url, icon_url);
  }
}

base::WeakPtr<FaviconCache> FaviconCache::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

size_t FaviconCache::NumTasksForTest() const {
  return page_task_map_.size();
}

size_t FaviconCache::NumFaviconMappingsForTest() const {
  return page_favicon_map_.size();
}

bool FaviconCache::FaviconRecencyFunctor::operator()(
    const FaviconInfo* lhs,
    const FaviconInfo* rhs) const {
  if (lhs->last_visit_time < rhs->last_visit_time)
    return true;
  else if (lhs->last_visit_time == rhs->last_visit_time)
    return lhs->favicon_url.spec() < rhs->favicon_url.spec();
  return false;
}

void FaviconCache::OnFaviconDataAvailable(
    const GURL& page_url,
    const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_results) {
  auto page_iter = page_task_map_.find(page_url);
  if (page_iter == page_task_map_.end())
    return;
  page_task_map_.erase(page_iter);

  if (bitmap_results.size() == 0) {
    // Either the favicon isn't loaded yet or there is no valid favicon.
    // We already cleared the task id, so just return.
    DVLOG(1) << "Favicon load failed for page " << page_url.spec();
    return;
  }

  for (size_t i = 0; i < bitmap_results.size(); ++i) {
    const favicon_base::FaviconRawBitmapResult& bitmap_result =
        bitmap_results[i];
    GURL favicon_url = bitmap_result.icon_url;
    if (!favicon_url.is_valid() || favicon_url.SchemeIs("data"))
      continue;  // Can happen if the page is still loading.

    SetIconUrlForPageUrl(page_url, favicon_url);
  }
}

void FaviconCache::SetIconUrlForPageUrl(const GURL& page_url,
                                        const GURL& favicon_url) {
  DCHECK_EQ(recent_mappings_.size(), page_favicon_map_.size());

  // If |page_url| is mapped, remove it's current mapping from the recency set.
  const auto& iter = page_favicon_map_.find(page_url);
  if (iter != page_favicon_map_.end()) {
    FaviconInfo* old_info = iter->second.get();
    recent_mappings_.erase(old_info);
  }

  DVLOG(1) << "Associating " << page_url.spec() << " with favicon at "
           << favicon_url.spec();
  auto new_info = std::make_unique<FaviconInfo>(favicon_url);
  recent_mappings_.insert(new_info.get());
  page_favicon_map_[page_url] = std::move(new_info);
  DCHECK_EQ(recent_mappings_.size(), page_favicon_map_.size());

  // Expire old mappings (if needed).
  while (recent_mappings_.size() > max_mappings_limit_) {
    FaviconInfo* candidate = *recent_mappings_.begin();
    DVLOG(1) << "Expiring favicon " << candidate->favicon_url.spec();
    recent_mappings_.erase(recent_mappings_.begin());
    base::EraseIf(page_favicon_map_, [candidate](const auto& kv) {
      return kv.second.get() == candidate;
    });
  }
  DCHECK_EQ(recent_mappings_.size(), page_favicon_map_.size());
}

void FaviconCache::OnURLsDeleted(history::HistoryService* history_service,
                                 const history::DeletionInfo& deletion_info) {
  // We only care about actual user (or sync) deletions.
  if (deletion_info.is_from_expiration())
    return;

  if (!deletion_info.IsAllHistory()) {
    for (const GURL& favicon_url : deletion_info.favicon_urls()) {
      DVLOG(1) << "Dropping mapping for favicon " << favicon_url;
      base::EraseIf(recent_mappings_, [favicon_url](const FaviconInfo* info) {
        return info->favicon_url == favicon_url;
      });
      base::EraseIf(page_favicon_map_, [favicon_url](const auto& kv) {
        return kv.second->favicon_url == favicon_url;
      });
    }
    return;
  }

  // All history was cleared: just delete all mappings.
  DVLOG(1) << "History clear detected, deleting all mappings.";
  recent_mappings_.clear();
  page_favicon_map_.clear();
}

}  // namespace sync_sessions
