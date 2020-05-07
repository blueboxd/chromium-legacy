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
#include "chrome/browser/media/history/media_history_feed_associated_origins_table.h"
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
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

namespace {

constexpr int kCurrentVersionNumber = 1;
constexpr int kCompatibleVersionNumber = 1;

constexpr base::FilePath::CharType kMediaHistoryDatabaseName[] =
    FILE_PATH_LITERAL("Media History");

void DatabaseErrorCallback(sql::Database* db,
                           const base::FilePath& db_path,
                           int extended_error,
                           sql::Statement* stmt) {
  if (sql::Recovery::ShouldRecover(extended_error)) {
    // Prevent reentrant calls.
    db->reset_error_callback();

    // After this call, the |db| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabase(db, db_path);

    // The DLOG(FATAL) below is intended to draw immediate attention to errors
    // in newly-written code.  Database corruption is generally a result of OS
    // or hardware issues, not coding errors at the client level, so displaying
    // the error would probably lead to confusion.  The ignored call signals the
    // test-expectation framework that the error was handled.
    ignore_result(sql::Database::IsExpectedSqliteError(extended_error));
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db->GetErrorMessage();
}

base::FilePath GetDBPath(Profile* profile) {
  // If this is a testing profile then we should use an in-memory database.
  if (profile->AsTestingProfile())
    return base::FilePath();
  return profile->GetPath().Append(kMediaHistoryDatabaseName);
}

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
      db_path_(GetDBPath(profile)),
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
      feed_origins_table_(
          media_feeds::MediaFeedsService::IsEnabled()
              ? new MediaHistoryFeedAssociatedOriginsTable(db_task_runner_)
              : nullptr),
      initialization_successful_(false) {}

MediaHistoryStore::~MediaHistoryStore() {
  // The connection pointer needs to be deleted on the DB sequence since there
  // might be a task in progress on the DB sequence which uses this connection.
  if (meta_table_)
    db_task_runner_->DeleteSoon(FROM_HERE, meta_table_.release());
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
  if (!CanAccessDatabase())
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

void MediaHistoryStore::Initialize(const bool should_reset) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (should_reset) {
    if (!sql::Database::Delete(db_path_)) {
      LOG(ERROR) << "Failed to delete the old database.";

      base::UmaHistogramEnumeration(
          MediaHistoryStore::kInitResultHistogramName,
          MediaHistoryStore::InitResult::kFailedToDeleteOldDatabase);

      return;
    }
  }

  if (IsCancelled())
    return;

  auto result = InitializeInternal();

  if (IsCancelled()) {
    meta_table_.reset();
    db_.reset();
    return;
  }

  base::UmaHistogramEnumeration(MediaHistoryStore::kInitResultHistogramName,
                                result);
}

MediaHistoryStore::InitResult MediaHistoryStore::InitializeInternal() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  db_ = std::make_unique<sql::Database>();
  db_->set_histogram_tag("MediaHistory");
  db_->set_exclusive_locking();

  // To recover from corruption.
  db_->set_error_callback(
      base::BindRepeating(&DatabaseErrorCallback, db_.get(), db_path_));

  meta_table_ = std::make_unique<sql::MetaTable>();

  if (db_path_.empty()) {
    if (IsCancelled() || !db_ || !db_->OpenInMemory()) {
      LOG(ERROR) << "Failed to open the in-memory database.";

      return MediaHistoryStore::InitResult::kFailedToOpenDatabase;
    }
  } else {
    base::File::Error err;
    if (IsCancelled() ||
        !base::CreateDirectoryAndGetError(db_path_.DirName(), &err)) {
      LOG(ERROR) << "Failed to create the directory.";

      return MediaHistoryStore::InitResult::kFailedToCreateDirectory;
    }

    if (IsCancelled() || !db_ || !db_->Open(db_path_)) {
      LOG(ERROR) << "Failed to open the database.";

      return MediaHistoryStore::InitResult::kFailedToOpenDatabase;
    }
  }

  db_->Preload();

  if (IsCancelled() || !db_ || !db_->Execute("PRAGMA foreign_keys=1")) {
    LOG(ERROR) << "Failed to enable foreign keys on the media history store.";

    return MediaHistoryStore::InitResult::kFailedNoForeignKeys;
  }

  if (IsCancelled() || !db_ || !meta_table_ ||
      !meta_table_->Init(db_.get(), GetCurrentVersion(),
                         kCompatibleVersionNumber)) {
    LOG(ERROR) << "Failed to create the meta table.";

    return MediaHistoryStore::InitResult::kFailedToCreateMetaTable;
  }

  if (IsCancelled() || !db_ || !db_->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";

    return MediaHistoryStore::InitResult::kFailedToEstablishTransaction;
  }

  sql::InitStatus status = CreateOrUpgradeIfNeeded();
  if (status != sql::INIT_OK) {
    LOG(ERROR) << "Failed to create or update the media history store.";

    return MediaHistoryStore::InitResult::kFailedDatabaseTooNew;
  }

  status = InitializeTables();
  if (status != sql::INIT_OK) {
    LOG(ERROR) << "Failed to initialize the media history store tables.";

    return MediaHistoryStore::InitResult::kFailedInitializeTables;
  }

  if (IsCancelled() || !db_ || !DB()->CommitTransaction()) {
    LOG(ERROR) << "Failed to commit transaction.";

    return MediaHistoryStore::InitResult::kFailedToCommitTransaction;
  }

  initialization_successful_ = true;

  // Get the size in bytes.
  int64_t file_size = 0;
  base::GetFileSize(db_path_, &file_size);

  // Record the size in KB.
  if (file_size > 0) {
    base::UmaHistogramMemoryKB(MediaHistoryStore::kDatabaseSizeKbHistogramName,
                               file_size / 1000);
  }

  return MediaHistoryStore::InitResult::kSuccess;
}

sql::InitStatus MediaHistoryStore::CreateOrUpgradeIfNeeded() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (IsCancelled() || !meta_table_)
    return sql::INIT_FAILURE;

  int cur_version = meta_table_->GetVersionNumber();
  if (meta_table_->GetCompatibleVersionNumber() > kCurrentVersionNumber) {
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
  if (IsCancelled() || !db_)
    return sql::INIT_FAILURE;

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
  if (feed_origins_table_ && status == sql::INIT_OK)
    status = feed_origins_table_->Initialize(db_.get());

  return status;
}

bool MediaHistoryStore::CreateOriginId(const url::Origin& origin) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return false;

  return origin_table_->CreateOriginId(origin);
}

mojom::MediaHistoryStatsPtr MediaHistoryStore::GetMediaHistoryStats() {
  mojom::MediaHistoryStatsPtr stats(mojom::MediaHistoryStats::New());

  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
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
  if (!CanAccessDatabase())
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
  if (!CanAccessDatabase())
    return std::vector<mojom::MediaHistoryPlaybackRowPtr>();

  return playback_table_->GetPlaybackRows();
}

std::vector<media_feeds::mojom::MediaFeedPtr> MediaHistoryStore::GetMediaFeeds(
    const MediaHistoryKeyedService::GetMediaFeedsRequest& request) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase() || !feeds_table_ || !feed_origins_table_)
    return std::vector<media_feeds::mojom::MediaFeedPtr>();

  auto feeds = feeds_table_->GetRows(request);

  for (auto& feed : feeds) {
    feed->associated_origins = feed_origins_table_->Get(feed->id);
  }

  return feeds;
}

int MediaHistoryStore::GetTableRowCount(const std::string& table_name) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
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
  if (!CanAccessDatabase())
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

  if (!CanAccessDatabase())
    return std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>();

  auto sessions =
      session_table_->GetPlaybackSessions(num_sessions, std::move(filter));

  for (auto& session : sessions) {
    session->artwork = session_images_table_->GetImagesForSession(session->id);
  }

  return sessions;
}

void MediaHistoryStore::DeleteAllOriginData(
    const std::set<url::Origin>& origins) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
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
  if (!CanAccessDatabase())
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  MediaHistoryTableBase* tables[] = {
      playback_table_.get(),
      session_table_.get(),
  };

  std::set<url::Origin> origins_with_deletions;
  for (auto& url : urls) {
    origins_with_deletions.insert(url::Origin::Create(url));

    for (auto* table : tables) {
      if (!table->DeleteURL(url)) {
        DB()->RollbackTransaction();
        return;
      }
    }
  }

  for (auto& origin : origins_with_deletions) {
    if (!origin_table_->RecalculateAggregateAudioVideoWatchTime(origin)) {
      DB()->RollbackTransaction();
      return;
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
  if (!CanAccessDatabase())
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
  if (!CanAccessDatabase())
    return;

  if (!feeds_table_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  if (!(CreateOriginId(url::Origin::Create(url)) &&
        feeds_table_->DiscoverFeed(url))) {
    DB()->RollbackTransaction();
    return;
  }

  DB()->CommitTransaction();
}

void MediaHistoryStore::StoreMediaFeedFetchResult(
    MediaHistoryKeyedService::MediaFeedFetchResult result) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return;

  if (!feeds_table_ || !feed_items_table_ || !feed_origins_table_)
    return;

  auto fetch_details = feeds_table_->GetFetchDetails(result.feed_id);
  if (!fetch_details)
    return;

  // If the reset token does not match then we should store a fetch failure.
  if (fetch_details->reset_token != result.reset_token) {
    MediaHistoryKeyedService::MediaFeedFetchResult new_result;
    new_result.feed_id = result.feed_id;
    new_result.status =
        media_feeds::mojom::FetchResult::kFailedDueToResetWhileInflight;
    StoreMediaFeedFetchResultInternal(std::move(new_result));
    return;
  }

  StoreMediaFeedFetchResultInternal(std::move(result));
}

void MediaHistoryStore::StoreMediaFeedFetchResultInternal(
    MediaHistoryKeyedService::MediaFeedFetchResult result) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return;

  if (!feeds_table_ || !feed_items_table_ || !feed_origins_table_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  std::set<url::Origin> origins_set;
  for (auto& origin : result.associated_origins)
    origins_set.insert(origin);

  // Get the origin for the feed.
  auto origin = feeds_table_->GetOrigin(result.feed_id);
  if (!origin.has_value()) {
    DB()->RollbackTransaction();
    return;
  }
  origins_set.insert(*origin);

  // Remove all the items currently associated with this feed.
  if (!feed_items_table_->DeleteItems(result.feed_id)) {
    DB()->RollbackTransaction();
    return;
  }

  int item_play_next_count = 0;
  int item_content_types = 0;
  int item_safe_count = 0;

  for (auto& item : result.items) {
    // Save each item to the table.
    if (!feed_items_table_->SaveItem(result.feed_id, item)) {
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

    // If the item is marked as safe then we should add it to the safe count.
    if (item->safe_search_result ==
        media_feeds::mojom::SafeSearchResult::kSafe) {
      item_safe_count++;
    }

    item_content_types |= static_cast<int>(item->type);
  }

  const media_feeds::mojom::UserIdentifier* user_identifier =
      result.user_identifier ? result.user_identifier.get() : nullptr;

  // Update the metadata associated with this feed.
  if (!feeds_table_->UpdateFeedFromFetch(
          result.feed_id, result.status, result.was_fetched_from_cache,
          result.items.size(), item_play_next_count, item_content_types,
          result.logos, user_identifier, result.display_name,
          item_safe_count)) {
    DB()->RollbackTransaction();
    return;
  }

  // Clear any old associated origins.
  if (!feed_origins_table_->Clear(result.feed_id)) {
    DB()->RollbackTransaction();
    return;
  }

  // Store associated origins.
  for (auto& origin : origins_set) {
    if (!feed_origins_table_->Add(origin, result.feed_id)) {
      DB()->RollbackTransaction();
      return;
    }
  }

  if (result.status !=
      media_feeds::mojom::FetchResult::kFailedDueToResetWhileInflight) {
    if (!feeds_table_->ClearResetReason(result.feed_id)) {
      DB()->RollbackTransaction();
      return;
    }
  }

  DB()->CommitTransaction();
}

std::vector<media_feeds::mojom::MediaFeedItemPtr>
MediaHistoryStore::GetItemsForMediaFeedForDebug(const int64_t feed_id) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (!CanAccessDatabase() || !feed_items_table_)
    return std::vector<media_feeds::mojom::MediaFeedItemPtr>();

  return feed_items_table_->GetItemsForFeed(feed_id);
}

MediaHistoryKeyedService::PendingSafeSearchCheckList
MediaHistoryStore::GetPendingSafeSearchCheckMediaFeedItems() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (!CanAccessDatabase() || !feed_items_table_)
    return MediaHistoryKeyedService::PendingSafeSearchCheckList();

  return feed_items_table_->GetPendingSafeSearchCheckItems();
}

void MediaHistoryStore::StoreMediaFeedItemSafeSearchResults(
    std::map<int64_t, media_feeds::mojom::SafeSearchResult> results) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return;

  if (!feeds_table_ || !feed_items_table_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  std::set<int64_t> feed_ids;
  for (auto& entry : results) {
    auto feed_id =
        feed_items_table_->StoreSafeSearchResult(entry.first, entry.second);

    if (!feed_id.has_value()) {
      DB()->RollbackTransaction();
      return;
    }

    feed_ids.insert(*feed_id);
  }

  for (auto& feed_id : feed_ids) {
    if (!feeds_table_->RecalculateSafeSearchItemCount(feed_id)) {
      DB()->RollbackTransaction();
      return;
    }
  }

  DB()->CommitTransaction();
}

void MediaHistoryStore::SetCancelled() {
  DCHECK(!db_task_runner_->RunsTasksInCurrentSequence());

  cancelled_.Set();

  MediaHistoryTableBase* tables[] = {
      origin_table_.get(),         playback_table_.get(), session_table_.get(),
      session_images_table_.get(), images_table_.get(),   feeds_table_.get(),
      feed_items_table_.get(),
  };

  for (auto* table : tables) {
    if (table)
      table->SetCancelled();
  }
}

void MediaHistoryStore::IncrementMediaFeedItemsShownCount(
    const std::set<int64_t> feed_item_ids) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return;

  if (!feed_items_table_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  for (auto& feed_item_id : feed_item_ids) {
    if (!feed_items_table_->IncrementShownCount(feed_item_id)) {
      DB()->RollbackTransaction();
      return;
    }
  }

  DB()->CommitTransaction();
}

void MediaHistoryStore::MarkMediaFeedItemAsClicked(
    const int64_t& feed_item_id) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return;

  if (!feed_items_table_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  if (feed_items_table_->MarkAsClicked(feed_item_id)) {
    DB()->CommitTransaction();
  } else {
    DB()->RollbackTransaction();
  }
}

bool MediaHistoryStore::CanAccessDatabase() const {
  return !IsCancelled() && initialization_successful_ && db_ && db_->is_open();
}

bool MediaHistoryStore::IsCancelled() const {
  return cancelled_.IsSet();
}

void MediaHistoryStore::UpdateMediaFeedDisplayTime(const int64_t feed_id) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return;

  if (!feeds_table_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  if (!feeds_table_->UpdateDisplayTime(feed_id)) {
    DB()->RollbackTransaction();
    return;
  }

  DB()->CommitTransaction();
}

void MediaHistoryStore::ResetMediaFeed(const url::Origin& origin,
                                       media_feeds::mojom::ResetReason reason,
                                       const bool include_subdomains) {
  if (!CanAccessDatabase())
    return;

  if (!feeds_table_ || !feed_items_table_ || !feed_origins_table_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  // Get all the feeds with |origin| as an associated origin.
  std::set<int64_t> feed_ids =
      feed_origins_table_->GetFeeds(origin, include_subdomains);

  if (ResetMediaFeedInternal(feed_ids, reason)) {
    DB()->CommitTransaction();
  } else {
    DB()->RollbackTransaction();
  }
}

void MediaHistoryStore::ResetMediaFeedDueToCacheClearing(
    const base::Time& start_time,
    const base::Time& end_time,
    MediaHistoryKeyedService::CacheClearingFilter filter) {
  if (!CanAccessDatabase())
    return;

  if (!feeds_table_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  const auto start_time_s = start_time.ToDeltaSinceWindowsEpoch().InSeconds();
  const auto end_time_s = end_time.ToDeltaSinceWindowsEpoch().InSeconds();

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, url FROM mediaFeed WHERE last_fetch_time_s >= ? AND "
      "last_fetch_time_s <= ?"));
  statement.BindInt64(0, start_time_s);
  statement.BindInt64(1, end_time_s);

  std::set<int64_t> feed_ids;
  while (statement.Step()) {
    GURL url(statement.ColumnString(1));

    if (!filter.is_null() && !filter.Run(url))
      continue;

    feed_ids.insert(statement.ColumnInt64(0));
  }

  if (ResetMediaFeedInternal(feed_ids,
                             media_feeds::mojom::ResetReason::kCache)) {
    DB()->CommitTransaction();
  } else {
    DB()->RollbackTransaction();
  }
}

bool MediaHistoryStore::ResetMediaFeedInternal(
    const std::set<int64_t>& feed_ids,
    media_feeds::mojom::ResetReason reason) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  for (auto& feed_id : feed_ids) {
    // Remove all the items currently associated with this feed.
    if (!feeds_table_->Reset(feed_id, reason))
      return false;

    // Remove all the items currently associated with this feed.
    if (!feed_items_table_->DeleteItems(feed_id))
      return false;

    // Clear any old associated origins.
    if (!feed_origins_table_->Clear(feed_id))
      return false;

    auto feed_origin = feeds_table_->GetOrigin(feed_id);
    if (!feed_origin)
      return false;

    // Add the origin of the feed back so we will update the reset tokens if
    // we are reset again before the next fetch.
    if (!feed_origins_table_->Add(*feed_origin, feed_id))
      return false;
  }

  return true;
}

void MediaHistoryStore::DeleteMediaFeed(const int64_t feed_id) {
  if (!CanAccessDatabase())
    return;

  if (!feeds_table_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  if (!feeds_table_->Delete(feed_id)) {
    DB()->RollbackTransaction();
    return;
  }

  DB()->CommitTransaction();
}

base::Optional<MediaHistoryKeyedService::MediaFeedFetchDetails>
MediaHistoryStore::GetMediaFeedFetchDetails(const int64_t feed_id) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase() || !feeds_table_)
    return base::nullopt;

  return feeds_table_->GetFetchDetails(feed_id);
}

}  // namespace media_history
