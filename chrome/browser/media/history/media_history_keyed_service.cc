// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_keyed_service.h"

#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_context.h"
#include "media/base/media_switches.h"

namespace media_history {

MediaHistoryKeyedService::MediaHistoryKeyedService(Profile* profile)
    : profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());

  // May be null in tests.
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  if (history)
    history->AddObserver(this);

  auto db_task_runner = base::ThreadPool::CreateUpdateableSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  media_history_store_ =
      std::make_unique<MediaHistoryStore>(profile_, std::move(db_task_runner));
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
  if (deletion_info.IsAllHistory()) {
    // Destroy the old database and create a new one.
    media_history_store_->EraseDatabaseAndCreateNew();
    return;
  }

  // Build a set of all origins in |deleted_rows|.
  std::set<url::Origin> origins;
  for (const history::URLRow& row : deletion_info.deleted_rows()) {
    origins.insert(url::Origin::Create(row.url()));
  }

  // Find any origins that do not have any more data in the history database.
  std::set<url::Origin> no_more_origins;
  for (const url::Origin& origin : origins) {
    const auto& origin_count =
        deletion_info.deleted_urls_origin_map().find(origin.GetURL());

    if (origin_count->second.first > 0)
      continue;

    no_more_origins.insert(origin);
  }

  if (!no_more_origins.empty())
    media_history_store_->DeleteAllOriginData(no_more_origins);

  // TODO(https://crbug.com/1024352): For any origins that still have data we
  // should remove data by URL.
}

void MediaHistoryKeyedService::SavePlayback(
    const content::MediaPlayerWatchTime& watch_time) {
  media_history_store_->SavePlayback(watch_time);
}

void MediaHistoryKeyedService::GetMediaHistoryStats(
    base::OnceCallback<void(mojom::MediaHistoryStatsPtr)> callback) {
  media_history_store_->GetMediaHistoryStats(std::move(callback));
}

void MediaHistoryKeyedService::GetOriginRowsForDebug(
    base::OnceCallback<void(std::vector<mojom::MediaHistoryOriginRowPtr>)>
        callback) {
  media_history_store_->GetOriginRowsForDebug(std::move(callback));
}

void MediaHistoryKeyedService::GetMediaHistoryPlaybackRowsForDebug(
    base::OnceCallback<void(std::vector<mojom::MediaHistoryPlaybackRowPtr>)>
        callback) {
  media_history_store_->GetMediaHistoryPlaybackRowsForDebug(
      std::move(callback));
}

void MediaHistoryKeyedService::GetPlaybackSessions(
    base::Optional<unsigned int> num_sessions,
    base::Optional<GetPlaybackSessionsFilter> filter,
    base::OnceCallback<
        void(std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>)> callback) {
  media_history_store_->GetPlaybackSessions(num_sessions, filter,
                                            std::move(callback));
}

void MediaHistoryKeyedService::SavePlaybackSession(
    const GURL& url,
    const media_session::MediaMetadata& metadata,
    const base::Optional<media_session::MediaPosition>& position,
    const std::vector<media_session::MediaImage>& artwork) {
  media_history_store_->SavePlaybackSession(url, metadata, position, artwork);
}

void MediaHistoryKeyedService::GetURLsInTableForTest(
    const std::string& table,
    base::OnceCallback<void(std::set<GURL>)> callback) {
  media_history_store_->GetURLsInTableForTest(table, std::move(callback));
}

void MediaHistoryKeyedService::SaveMediaFeed(const GURL& url) {
  media_history_store_->SaveMediaFeed(url);
}

}  // namespace media_history
