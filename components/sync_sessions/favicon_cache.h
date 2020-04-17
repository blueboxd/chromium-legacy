// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_FAVICON_CACHE_H_
#define COMPONENTS_SYNC_SESSIONS_FAVICON_CACHE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "url/gurl.h"

namespace favicon_base {
struct FaviconRawBitmapResult;
}  // namespace favicon_base

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace history {
class DeletionInfo;
}  // namespace history

namespace sync_pb {
class SessionTab;
}  // namespace sync_pb

namespace sync_sessions {

struct FaviconInfo;

// FAVICON SYNC IS DEPRECATED: This class now only serves to the translation
// from page url to icon url using sessions sync information.
// TODO(https://crbug.com/978775): Rename the class accordingly.
class FaviconCache : public history::HistoryServiceObserver {
 public:
  FaviconCache(favicon::FaviconService* favicon_service,
               history::HistoryService* history_service,
               int max_mappings_limit);
  ~FaviconCache() override;

  // Returns the value associated with |page_url| in |page_favicon_map_| if one
  // exists, otherwise returns an empty URL.
  GURL GetIconUrlForPageUrl(const GURL& page_url) const;

  // Load the favicon mapping for |page_url|.
  void OnPageFaviconUpdated(const GURL& page_url);

  // Update the visit count for the favicon associated with |favicon_url|.
  // If no favicon exists associated with |favicon_url|, triggers a load
  // for the favicon associated with |page_url|.
  void OnFaviconVisited(const GURL& page_url, const GURL& favicon_url);

  // Consume Session sync favicon data to update the in-memory page->favicon url
  // mappings and visit times.
  void UpdateMappingsFromForeignTab(const sync_pb::SessionTab& tab);

  base::WeakPtr<FaviconCache> GetWeakPtr();

  // For testing only.
  size_t NumTasksForTest() const;
  size_t NumFaviconMappingsForTest() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SyncFaviconCacheTest, HistoryFullClear);
  FRIEND_TEST_ALL_PREFIXES(SyncFaviconCacheTest, HistorySubsetClear);

  // Functor for ordering FaviconInfo objects by recency;
  struct FaviconRecencyFunctor {
    bool operator()(const FaviconInfo* lhs, const FaviconInfo* rhs) const;
  };

  using RecencySet = std::set<FaviconInfo*, FaviconRecencyFunctor>;

  // Callback method to store a tab's favicon into its sync node once it becomes
  // available. Does nothing if no favicon data was available.
  void OnFaviconDataAvailable(
      const GURL& page_url,
      const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_result);

  // Stores mapping for given |page_url| and |favicon_url| and sets visit
  // time for this mapping to now. If there already was a mapping for
  // |page_url|, this previous mapping gets overwritten.
  void SetIconUrlForPageUrl(const GURL& page_url, const GURL& favicon_url);

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  favicon::FaviconService* const favicon_service_;

  // Task tracker for loading favicons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // An LRU ordering of the favicon mappings in |page_favicon_map_| (oldest
  // first).
  RecencySet recent_mappings_;

  // Pending favicon loads, map of page url to task id.
  std::map<GURL, base::CancelableTaskTracker::TaskId> page_task_map_;

  // Map of page url to favicon info.
  std::map<GURL, std::unique_ptr<FaviconInfo>> page_favicon_map_;

  // Maximum number of mappings to keep in memory. 0 means no limit.
  const size_t max_mappings_limit_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_{this};

  // Weak pointer factory for favicon loads.
  base::WeakPtrFactory<FaviconCache> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FaviconCache);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_FAVICON_CACHE_H_
