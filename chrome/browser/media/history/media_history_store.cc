// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/media/feeds/media_feeds_service.h"
#include "chrome/browser/media/history/media_history_feed_items_table.h"
#include "chrome/browser/media/history/media_history_feeds_table.h"
#include "chrome/browser/media/history/media_history_images_table.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_playback_table.h"
#include "chrome/browser/media/history/media_history_session_images_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "content/public/browser/media_player_watch_time.h"
#include "services/media_session/public/cpp/media_image.h"
#include "services/media_session/public/cpp/media_position.h"
#include "sql/statement.h"
#include "url/origin.h"

namespace {

constexpr int kCurrentVersionNumber = 1;
constexpr int kCompatibleVersionNumber = 1;

constexpr base::FilePath::CharType kMediaHistoryDatabaseName[] =
    FILE_PATH_LITERAL("Media History");

}  // namespace

int GetCurrentVersion() {
  return kCurrentVersionNumber;
}

namespace media_history {

const char MediaHistoryStore::kInitResultHistogramName[] =
    "Media.History.Init.Result";

const char MediaHistoryStore::kPlaybackWriteResultHistogramName[] =
    "Media.History.Playback.WriteResult";

const char MediaHistoryStore::kSessionWriteResultHistogramName[] =
    "Media.History.Session.WriteResult";

const char MediaHistoryStore::kDatabaseSizeKbHistogramName[] =
    "Media.History.DatabaseSize";

MediaHistoryStore::MediaHistoryStore(
    Profile* profile,
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : db_task_runner_(db_task_runner),
      db_path_(profile->GetPath().Append(kMediaHistoryDatabaseName)),
      origin_table_(new MediaHistoryOriginTable(db_task_runner_)),
      playback_table_(new MediaHistoryPlaybackTable(db_task_runner_)),
      session_table_(new MediaHistorySessionTable(db_task_runner_)),
      session_images_table_(
          new MediaHistorySessionImagesTable(db_task_runner_)),
      images_table_(new MediaHistoryImagesTable(db_task_runner_)),
      feeds_table_(media_feeds::MediaFeedsService::IsEnabled()
                       ? new MediaHistoryFeedsTable(db_task_runner_)
                       : nullptr),
      feed_items_table_(media_feeds::MediaFeedsService::IsEnabled()
                            ? new MediaHistoryFeedItemsTable(db_task_runner_)
                            : nullptr),
      initialization_successful_(false) {
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaHistoryStore::Initialize, base::Unretained(this)));
}

MediaHistoryStore::~MediaHistoryStore() {
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(origin_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(playback_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(session_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(session_images_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(images_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(feeds_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(feed_items_table_));

  if (db_)
    db_task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

sql::Database* MediaHistoryStore::DB() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return db_.get();
}

void MediaHistoryStore::SavePlayback(
    const content::MediaPlayerWatchTime& watch_time) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kPlaybackWriteResultHistogramName,
        MediaHistoryStore::PlaybackWriteResult::kFailedToEstablishTransaction);

    return;
  }

  // TODO(https://crbug.com/1052436): Remove the separate origin.
  auto origin = url::Origin::Create(watch_time.origin);
  CHECK_EQ(origin, url::Origin::Create(watch_time.url));

  if (!CreateOriginId(origin)) {
    DB()->RollbackTransaction();

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kPlaybackWriteResultHistogramName,
        MediaHistoryStore::PlaybackWriteResult::kFailedToWriteOrigin);

    return;
  }

  if (!playback_table_->SavePlayback(watch_time)) {
    DB()->RollbackTransaction();

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kPlaybackWriteResultHistogramName,
        MediaHistoryStore::PlaybackWriteResult::kFailedToWritePlayback);

    return;
  }

  if (watch_time.has_audio && watch_time.has_video) {
    if (!origin_table_->IncrementAggregateAudioVideoWatchTime(
            origin, watch_time.cumulative_watch_time)) {
      DB()->RollbackTransaction();

      base::UmaHistogramEnumeration(
          MediaHistoryStore::kPlaybackWriteResultHistogramName,
          MediaHistoryStore::PlaybackWriteResult::
              kFailedToIncrementAggreatedWatchtime);

      return;
    }
  }

  DB()->CommitTransaction();

  base::UmaHistogramEnumeration(
      MediaHistoryStore::kPlaybackWriteResultHistogramName,
      MediaHistoryStore::PlaybackWriteResult::kSuccess);
}

void MediaHistoryStore::Initialize() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  db_ = std::make_unique<sql::Database>();
  db_->set_histogram_tag("MediaHistory");

  base::File::Error err;
  if (!base::CreateDirectoryAndGetError(db_path_.DirName(), &err)) {
    LOG(ERROR) << "Failed to create the directory.";

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kFailedToCreateDirectory);

    return;
  }

  if (!db_->Open(db_path_)) {
    LOG(ERROR) << "Failed to open the database.";

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kFailedToOpenDatabase);

    return;
  }

  db_->Preload();

  if (!db_->Execute("PRAGMA foreign_keys=1")) {
    LOG(ERROR) << "Failed to enable foreign keys on the media history store.";
    db_->Poison();

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kFailedNoForeignKeys);

    return;
  }

  if (!meta_table_.Init(db_.get(), GetCurrentVersion(),
                        kCompatibleVersionNumber)) {
    LOG(ERROR) << "Failed to create the meta table.";

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kFailedToCreateMetaTable);

    return;
  }

  if (!db_->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kFailedToEstablishTransaction);

    return;
  }

  sql::InitStatus status = CreateOrUpgradeIfNeeded();
  if (status != sql::INIT_OK) {
    LOG(ERROR) << "Failed to create or update the media history store.";

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kFailedDatabaseTooNew);

    return;
  }

  status = InitializeTables();
  if (status != sql::INIT_OK) {
    LOG(ERROR) << "Failed to initialize the media history store tables.";

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kFailedInitializeTables);

    return;
  }

  // Commit the transaction for creating the db_->
  DB()->CommitTransaction();

  initialization_successful_ = true;

  base::UmaHistogramEnumeration(MediaHistoryStore::kInitResultHistogramName,
                                MediaHistoryStore::InitResult::kSuccess);

  // Get the size in bytes.
  int64_t file_size = 0;
  base::GetFileSize(db_path_, &file_size);

  // Record the size in KB.
  if (file_size > 0) {
    base::UmaHistogramMemoryKB(MediaHistoryStore::kDatabaseSizeKbHistogramName,
                               file_size / 1000);
  }
}

sql::InitStatus MediaHistoryStore::CreateOrUpgradeIfNeeded() {
  if (!db_->is_open())
    return sql::INIT_FAILURE;

  int cur_version = meta_table_.GetVersionNumber();
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Media history database is too new.";
    return sql::INIT_TOO_NEW;
  }

  LOG_IF(WARNING, cur_version < GetCurrentVersion())
      << "Media history database version " << cur_version
      << " is too old to handle.";

  return sql::INIT_OK;
}

sql::InitStatus MediaHistoryStore::InitializeTables() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  sql::InitStatus status = origin_table_->Initialize(db_.get());
  if (status == sql::INIT_OK)
    status = playback_table_->Initialize(db_.get());
  if (status == sql::INIT_OK)
    status = session_table_->Initialize(db_.get());
  if (status == sql::INIT_OK)
    status = session_images_table_->Initialize(db_.get());
  if (status == sql::INIT_OK)
    status = images_table_->Initialize(db_.get());
  if (feeds_table_ && status == sql::INIT_OK)
    status = feeds_table_->Initialize(db_.get());
  if (feed_items_table_ && status == sql::INIT_OK)
    status = feed_items_table_->Initialize(db_.get());

  return status;
}

bool MediaHistoryStore::CreateOriginId(const url::Origin& origin) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return false;

  return origin_table_->CreateOriginId(origin);
}

mojom::MediaHistoryStatsPtr MediaHistoryStore::GetMediaHistoryStats() {
  mojom::MediaHistoryStatsPtr stats(mojom::MediaHistoryStats::New());

  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return stats;

  sql::Statement statement(DB()->GetUniqueStatement(
      "SELECT name FROM sqlite_master WHERE type='table' "
      "AND name NOT LIKE 'sqlite_%';"));

  std::vector<std::string> table_names;
  while (statement.Step()) {
    auto table_name = statement.ColumnString(0);
    stats->table_row_counts.emplace(table_name, GetTableRowCount(table_name));
  }

  DCHECK(statement.Succeeded());
  return stats;
}

std::vector<mojom::MediaHistoryOriginRowPtr>
MediaHistoryStore::GetOriginRowsForDebug() {
  std::vector<mojom::MediaHistoryOriginRowPtr> origins;

  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return origins;

  sql::Statement statement(DB()->GetUniqueStatement(
      base::StringPrintf(
          "SELECT O.origin, O.last_updated_time_s, "
          "O.aggregate_watchtime_audio_video_s,  "
          "(SELECT SUM(watch_time_s) FROM %s WHERE origin_id = O.id AND "
          "has_video = 1 AND has_audio = 1) AS accurate_watchtime "
          "FROM %s O",
          MediaHistoryPlaybackTable::kTableName,
          MediaHistoryOriginTable::kTableName)
          .c_str()));

  std::vector<std::string> table_names;
  while (statement.Step()) {
    mojom::MediaHistoryOriginRowPtr origin(mojom::MediaHistoryOriginRow::New());

    origin->origin = url::Origin::Create(GURL(statement.ColumnString(0)));
    origin->last_updated_time =
        base::Time::FromDeltaSinceWindowsEpoch(
            base::TimeDelta::FromSeconds(statement.ColumnInt64(1)))
            .ToJsTime();
    origin->cached_audio_video_watchtime =
        base::TimeDelta::FromSeconds(statement.ColumnInt64(2));
    origin->actual_audio_video_watchtime =
        base::TimeDelta::FromSeconds(statement.ColumnInt64(3));

    origins.push_back(std::move(origin));
  }

  DCHECK(statement.Succeeded());
  return origins;
}

std::vector<mojom::MediaHistoryPlaybackRowPtr>
MediaHistoryStore::GetMediaHistoryPlaybackRowsForDebug() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return std::vector<mojom::MediaHistoryPlaybackRowPtr>();

  return playback_table_->GetPlaybackRows();
}

std::vector<media_feeds::mojom::MediaFeedPtr>
MediaHistoryStore::GetMediaFeedsForDebug() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_ || !feeds_table_)
    return std::vector<media_feeds::mojom::MediaFeedPtr>();

  return feeds_table_->GetRows();
}

int MediaHistoryStore::GetTableRowCount(const std::string& table_name) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return -1;

  sql::Statement statement(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT count(*) from %s", table_name.c_str())
          .c_str()));

  while (statement.Step()) {
    return statement.ColumnInt(0);
  }

  return -1;
}

void MediaHistoryStore::SavePlaybackSession(
    const GURL& url,
    const media_session::MediaMetadata& metadata,
    const base::Optional<media_session::MediaPosition>& position,
    const std::vector<media_session::MediaImage>& artwork) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kSessionWriteResultHistogramName,
        MediaHistoryStore::SessionWriteResult::kFailedToEstablishTransaction);

    return;
  }

  auto origin = url::Origin::Create(url);
  if (!CreateOriginId(origin)) {
    DB()->RollbackTransaction();

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kSessionWriteResultHistogramName,
        MediaHistoryStore::SessionWriteResult::kFailedToWriteOrigin);
    return;
  }

  auto session_id =
      session_table_->SavePlaybackSession(url, origin, metadata, position);
  if (!session_id) {
    DB()->RollbackTransaction();

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kSessionWriteResultHistogramName,
        MediaHistoryStore::SessionWriteResult::kFailedToWriteSession);
    return;
  }

  for (auto& image : artwork) {
    auto image_id =
        images_table_->SaveOrGetImage(image.src, origin, image.type);
    if (!image_id) {
      DB()->RollbackTransaction();

      base::UmaHistogramEnumeration(
          MediaHistoryStore::kSessionWriteResultHistogramName,
          MediaHistoryStore::SessionWriteResult::kFailedToWriteImage);
      return;
    }

    // If we do not have any sizes associated with the image we should save a
    // link with a null size. Otherwise, we should save a link for each size.
    if (image.sizes.empty()) {
      session_images_table_->LinkImage(*session_id, *image_id, base::nullopt);
    } else {
      for (auto& size : image.sizes) {
        session_images_table_->LinkImage(*session_id, *image_id, size);
      }
    }
  }

  DB()->CommitTransaction();

  base::UmaHistogramEnumeration(
      MediaHistoryStore::kSessionWriteResultHistogramName,
      MediaHistoryStore::SessionWriteResult::kSuccess);
}

std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>
MediaHistoryStore::GetPlaybackSessions(
    base::Optional<unsigned int> num_sessions,
    base::Optional<MediaHistoryStore::GetPlaybackSessionsFilter> filter) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (!initialization_successful_)
    return std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>();

  auto sessions =
      session_table_->GetPlaybackSessions(num_sessions, std::move(filter));

  for (auto& session : sessions) {
    session->artwork = session_images_table_->GetImagesForSession(session->id);
  }

  return sessions;
}

void MediaHistoryStore::RazeAndClose() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (db_->is_open())
    db_->RazeAndClose();

  sql::Database::Delete(db_path_);
}

void MediaHistoryStore::DeleteAllOriginData(
    const std::set<url::Origin>& origins) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  for (auto& origin : origins) {
    if (!origin_table_->Delete(origin)) {
      DB()->RollbackTransaction();
      return;
    }
  }

  DB()->CommitTransaction();
}

void MediaHistoryStore::DeleteAllURLData(const std::set<GURL>& urls) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  MediaHistoryTableBase* tables[] = {
      playback_table_.get(),
      session_table_.get(),
  };

  for (auto& url : urls) {
    for (auto* table : tables) {
      if (!table->DeleteURL(url)) {
        DB()->RollbackTransaction();
        return;
      }
    }
  }

  // The mediaImages table will not be automatically cleared when we remove
  // single sessions so we should remove them manually.
  sql::Statement statement(DB()->GetUniqueStatement(
      "DELETE FROM mediaImage WHERE id IN ("
      "  SELECT id FROM mediaImage LEFT JOIN sessionImage"
      "  ON sessionImage.image_id = mediaImage.id"
      "  WHERE sessionImage.session_id IS NULL)"));

  if (!statement.Run()) {
    DB()->RollbackTransaction();
  } else {
    DB()->CommitTransaction();
  }
}

std::set<GURL> MediaHistoryStore::GetURLsInTableForTest(
    const std::string& table) {
  std::set<GURL> urls;

  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return urls;

  sql::Statement statement(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT url from %s", table.c_str()).c_str()));

  while (statement.Step()) {
    urls.insert(GURL(statement.ColumnString(0)));
  }

  DCHECK(statement.Succeeded());
  return urls;
}

void MediaHistoryStore::DiscoverMediaFeed(const GURL& url) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  if (!feeds_table_)
    return;

  if (!(CreateOriginId(url::Origin::Create(url)) &&
        feeds_table_->DiscoverFeed(url))) {
    DB()->RollbackTransaction();
    return;
  }

  DB()->CommitTransaction();
}

void MediaHistoryStore::StoreMediaFeedFetchResult(
    const int64_t feed_id,
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items,
    const media_feeds::mojom::FetchResult result,
    const bool was_fetched_from_cache,
    const std::vector<media_session::MediaImage>& logos,
    const std::string& display_name) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  if (!feeds_table_ || !feed_items_table_)
    return;

  // Remove all the items currently associated with this feed.
  if (!feed_items_table_->DeleteItems(feed_id)) {
    DB()->RollbackTransaction();
    return;
  }

  int item_play_next_count = 0;
  int item_content_types = 0;

  for (auto& item : items) {
    // Save each item to the table.
    if (!feed_items_table_->SaveItem(feed_id, item)) {
      DB()->RollbackTransaction();
      return;
    }

    // If the item has a play next candidate or the user is currently watching
    // this media then we should add it to the play next count.
    if (item->play_next_candidate ||
        item->action_status ==
            media_feeds::mojom::MediaFeedItemActionStatus::kActive) {
      item_play_next_count++;
    }

    item_content_types |= static_cast<int>(item->type);
  }

  // Update the metadata associated with this feed.
  if (!feeds_table_->UpdateFeedFromFetch(
          feed_id, result, was_fetched_from_cache, items.size(),
          item_play_next_count, item_content_types, logos, display_name)) {
    DB()->RollbackTransaction();
    return;
  }

  DB()->CommitTransaction();
}

std::vector<media_feeds::mojom::MediaFeedItemPtr>
MediaHistoryStore::GetItemsForMediaFeedForDebug(const int64_t feed_id) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (!initialization_successful_ || !feed_items_table_)
    return std::vector<media_feeds::mojom::MediaFeedItemPtr>();

  return feed_items_table_->GetItemsForFeed(feed_id);
}

MediaHistoryKeyedService::PendingSafeSearchCheckList
MediaHistoryStore::GetPendingSafeSearchCheckMediaFeedItems() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (!initialization_successful_ || !feed_items_table_)
    return MediaHistoryKeyedService::PendingSafeSearchCheckList();

  return feed_items_table_->GetPendingSafeSearchCheckItems();
}

void MediaHistoryStore::StoreMediaFeedItemSafeSearchResults(
    std::map<int64_t, media_feeds::mojom::SafeSearchResult> results) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  if (!feed_items_table_)
    return;

  for (auto& entry : results) {
    if (!feed_items_table_->StoreSafeSearchResult(entry.first, entry.second)) {
      DB()->RollbackTransaction();
      return;
    }
  }

  DB()->CommitTransaction();
}

}  // namespace media_history
