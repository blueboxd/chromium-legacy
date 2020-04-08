// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_keyed_service.h"

#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "media/base/media_switches.h"

namespace media_history {

// StoreHolder will in most cases hold a local MediaHistoryStore. However, for
// OTR profiles we hold a pointer to the original profile store. When accessing
// MediaHistoryStore you should use GetForRead for read operations,
// GetForWrite for write operations and GetForDelete for delete operations.
// These can be null if the store is read only or we disable storing browsing
// history.
class MediaHistoryKeyedService::StoreHolder {
 public:
  explicit StoreHolder(Profile* profile,
                       std::unique_ptr<MediaHistoryStore> local)
      : profile_(profile), local_(std::move(local)) {}
  explicit StoreHolder(Profile* profile, MediaHistoryKeyedService* remote)
      : profile_(profile), remote_(remote) {}

  ~StoreHolder() = default;
  StoreHolder(const StoreHolder& t) = delete;
  StoreHolder& operator=(const StoreHolder&) = delete;

  MediaHistoryStore* GetForRead() {
    if (local_)
      return local_.get();
    return remote_->store_->GetForRead();
  }

  MediaHistoryStore* GetForWrite() {
    if (profile_->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled))
      return nullptr;
    if (local_)
      return local_.get();
    return nullptr;
  }

  MediaHistoryStore* GetForDelete() {
    if (local_)
      return local_.get();
    return nullptr;
  }

 private:
  Profile* profile_;
  std::unique_ptr<MediaHistoryStore> local_;
  MediaHistoryKeyedService* remote_ = nullptr;
};

MediaHistoryKeyedService::MediaHistoryKeyedService(Profile* profile)
    : profile_(profile) {
  // May be null in tests.
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  if (history)
    history->AddObserver(this);

  if (profile->IsOffTheRecord()) {
    MediaHistoryKeyedService* original =
        MediaHistoryKeyedService::Get(profile->GetOriginalProfile());
    DCHECK(original);

    store_ = std::make_unique<StoreHolder>(profile, original);
  } else {
    auto db_task_runner = base::ThreadPool::CreateUpdateableSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

    store_ = std::make_unique<StoreHolder>(
        profile_, std::make_unique<MediaHistoryStore>(
                      profile_, std::move(db_task_runner)));
  }
}

// static
MediaHistoryKeyedService* MediaHistoryKeyedService::Get(Profile* profile) {
  return MediaHistoryKeyedServiceFactory::GetForProfile(profile);
}

MediaHistoryKeyedService::~MediaHistoryKeyedService() = default;

bool MediaHistoryKeyedService::IsEnabled() {
  return base::FeatureList::IsEnabled(media::kUseMediaHistoryStore);
}

void MediaHistoryKeyedService::Shutdown() {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);
  if (history)
    history->RemoveObserver(this);
}

void MediaHistoryKeyedService::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // The store might not always be writable.
  auto* store = store_->GetForDelete();
  if (!store)
    return;

  if (deletion_info.IsAllHistory()) {
    // Destroy the old database and create a new one.
    store->EraseDatabaseAndCreateNew();
    return;
  }

  // Build a set of all urls and origins in |deleted_rows|.
  std::set<url::Origin> origins;
  for (const history::URLRow& row : deletion_info.deleted_rows()) {
    origins.insert(url::Origin::Create(row.url()));
  }

  // Find any origins that do not have any more data in the history database.
  std::set<url::Origin> deleted_origins;
  for (const url::Origin& origin : origins) {
    const auto& origin_count =
        deletion_info.deleted_urls_origin_map().find(origin.GetURL());

    if (origin_count->second.first > 0)
      continue;

    deleted_origins.insert(origin);
  }

  if (!deleted_origins.empty())
    store->DeleteAllOriginData(deleted_origins);

  // Build a set of all urls in |deleted_rows| that do not have their origin in
  // |deleted_origins|.
  std::set<GURL> deleted_urls;
  for (const history::URLRow& row : deletion_info.deleted_rows()) {
    auto origin = url::Origin::Create(row.url());

    if (base::Contains(deleted_origins, origin))
      continue;

    deleted_urls.insert(row.url());
  }

  if (!deleted_urls.empty())
    store->DeleteAllURLData(deleted_urls);
}

void MediaHistoryKeyedService::SavePlayback(
    const content::MediaPlayerWatchTime& watch_time) {
  if (auto* store = store_->GetForWrite())
    store->SavePlayback(watch_time);
}

void MediaHistoryKeyedService::GetMediaHistoryStats(
    base::OnceCallback<void(mojom::MediaHistoryStatsPtr)> callback) {
  store_->GetForRead()->GetMediaHistoryStats(std::move(callback));
}

void MediaHistoryKeyedService::GetOriginRowsForDebug(
    base::OnceCallback<void(std::vector<mojom::MediaHistoryOriginRowPtr>)>
        callback) {
  store_->GetForRead()->GetOriginRowsForDebug(std::move(callback));
}

void MediaHistoryKeyedService::GetMediaHistoryPlaybackRowsForDebug(
    base::OnceCallback<void(std::vector<mojom::MediaHistoryPlaybackRowPtr>)>
        callback) {
  store_->GetForRead()->GetMediaHistoryPlaybackRowsForDebug(
      std::move(callback));
}

void MediaHistoryKeyedService::GetPlaybackSessions(
    base::Optional<unsigned int> num_sessions,
    base::Optional<GetPlaybackSessionsFilter> filter,
    base::OnceCallback<
        void(std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>)> callback) {
  store_->GetForRead()->GetPlaybackSessions(num_sessions, filter,
                                            std::move(callback));
}

void MediaHistoryKeyedService::SavePlaybackSession(
    const GURL& url,
    const media_session::MediaMetadata& metadata,
    const base::Optional<media_session::MediaPosition>& position,
    const std::vector<media_session::MediaImage>& artwork) {
  if (auto* store = store_->GetForWrite())
    store->SavePlaybackSession(url, metadata, position, artwork);
}

void MediaHistoryKeyedService::GetItemsForMediaFeedForDebug(
    const int64_t feed_id,
    base::OnceCallback<void(std::vector<media_feeds::mojom::MediaFeedItemPtr>)>
        callback) {
  store_->GetForRead()->GetItemsForMediaFeedForDebug(feed_id,
                                                     std::move(callback));
}

void MediaHistoryKeyedService::StoreMediaFeedFetchResult(
    const int64_t feed_id,
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items,
    const media_feeds::mojom::FetchResult result,
    const bool was_fetched_from_cache,
    const std::vector<media_session::MediaImage>& logos,
    const std::string& display_name) {
  if (auto* store = store_->GetForWrite()) {
    store->StoreMediaFeedFetchResult(feed_id, std::move(items), result,
                                     was_fetched_from_cache, logos,
                                     display_name);
  }
}

void MediaHistoryKeyedService::GetURLsInTableForTest(
    const std::string& table,
    base::OnceCallback<void(std::set<GURL>)> callback) {
  store_->GetForRead()->GetURLsInTableForTest(table, std::move(callback));
}

void MediaHistoryKeyedService::DiscoverMediaFeed(const GURL& url) {
  if (auto* store = store_->GetForWrite())
    store->DiscoverMediaFeed(url);
}

MediaHistoryKeyedService::PendingSafeSearchCheck::PendingSafeSearchCheck(
    int64_t id)
    : id(id) {}

MediaHistoryKeyedService::PendingSafeSearchCheck::~PendingSafeSearchCheck() =
    default;

void MediaHistoryKeyedService::GetPendingSafeSearchCheckMediaFeedItems(
    base::OnceCallback<void(PendingSafeSearchCheckList)> callback) {
  store_->GetForRead()->GetPendingSafeSearchCheckMediaFeedItems(
      std::move(callback));
}

void MediaHistoryKeyedService::StoreMediaFeedItemSafeSearchResults(
    std::map<int64_t, media_feeds::mojom::SafeSearchResult> results) {
  if (auto* store = store_->GetForWrite())
    store->StoreMediaFeedItemSafeSearchResults(results);
}

void MediaHistoryKeyedService::PostTaskToDBForTest(base::OnceClosure callback) {
  store_->GetForRead()->PostTaskToDBForTest(std::move(callback));
}

void MediaHistoryKeyedService::GetMediaFeedsForDebug(
    base::OnceCallback<void(std::vector<media_feeds::mojom::MediaFeedPtr>)>
        callback) {
  store_->GetForRead()->GetMediaFeedsForDebug(std::move(callback));
}

}  // namespace media_history
