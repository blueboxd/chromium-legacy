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

// Refcounted as it is created, initialized and destroyed on a different thread
// from the DB sequence provided to the constructor of this class that is
// required for all methods performing database access.
class MediaHistoryStoreInternal
    : public base::RefCountedThreadSafe<MediaHistoryStoreInternal> {
 private:
  friend class base::RefCountedThreadSafe<MediaHistoryStoreInternal>;
  friend class MediaHistoryStore;

  explicit MediaHistoryStoreInternal(
      Profile* profile,
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  virtual ~MediaHistoryStoreInternal();

  // Opens the database file from the |db_path|. Separated from the
  // constructor to ease construction/destruction of this object on one thread
  // and database access on the DB sequence of |db_task_runner_|.
  void Initialize();

  sql::InitStatus CreateOrUpgradeIfNeeded();
  sql::InitStatus InitializeTables();
  sql::Database* DB();

  // Returns a flag indicating whether the origin id was created successfully.
  bool CreateOriginId(const url::Origin& origin);

  void SavePlayback(const content::MediaPlayerWatchTime& watch_time);

  mojom::MediaHistoryStatsPtr GetMediaHistoryStats();
  int GetTableRowCount(const std::string& table_name);

  std::vector<mojom::MediaHistoryOriginRowPtr> GetOriginRowsForDebug();

  std::vector<mojom::MediaHistoryPlaybackRowPtr>
  GetMediaHistoryPlaybackRowsForDebug();

  std::vector<media_feeds::mojom::MediaFeedPtr> GetMediaFeedsForDebug();

  void SavePlaybackSession(
      const GURL& url,
      const media_session::MediaMetadata& metadata,
      const base::Optional<media_session::MediaPosition>& position,
      const std::vector<media_session::MediaImage>& artwork);

  std::vector<mojom::MediaHistoryPlaybackSessionRowPtr> GetPlaybackSessions(
      base::Optional<unsigned int> num_sessions,
      base::Optional<MediaHistoryStore::GetPlaybackSessionsFilter> filter);

  void RazeAndClose();
  void DeleteAllOriginData(const std::set<url::Origin>& origins);
  void DeleteAllURLData(const std::set<GURL>& urls);

  std::set<GURL> GetURLsInTableForTest(const std::string& table);

  void DiscoverMediaFeed(const GURL& url);

  void StoreMediaFeedFetchResult(
      const int64_t feed_id,
      std::vector<media_feeds::mojom::MediaFeedItemPtr> items,
      const media_feeds::mojom::FetchResult result,
      const base::Time& expiry_time,
      const std::vector<media_session::MediaImage>& logos,
      const std::string& display_name);

  std::vector<media_feeds::mojom::MediaFeedItemPtr>
  GetItemsForMediaFeedForDebug(const int64_t feed_id);

  MediaHistoryKeyedService::PendingSafeSearchCheckList
  GetPendingSafeSearchCheckMediaFeedItems();

  void StoreMediaFeedItemSafeSearchResults(
      std::map<int64_t, media_feeds::mojom::SafeSearchResult> results);

  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;
  const base::FilePath db_path_;
  std::unique_ptr<sql::Database> db_;
  sql::MetaTable meta_table_;
  scoped_refptr<MediaHistoryOriginTable> origin_table_;
  scoped_refptr<MediaHistoryPlaybackTable> playback_table_;
  scoped_refptr<MediaHistorySessionTable> session_table_;
  scoped_refptr<MediaHistorySessionImagesTable> session_images_table_;
  scoped_refptr<MediaHistoryImagesTable> images_table_;
  scoped_refptr<MediaHistoryFeedsTable> feeds_table_;
  scoped_refptr<MediaHistoryFeedItemsTable> feed_items_table_;
  bool initialization_successful_;

  DISALLOW_COPY_AND_ASSIGN(MediaHistoryStoreInternal);
};

MediaHistoryStoreInternal::MediaHistoryStoreInternal(
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
      initialization_successful_(false) {}

MediaHistoryStoreInternal::~MediaHistoryStoreInternal() {
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(origin_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(playback_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(session_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(session_images_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(images_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(feeds_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(feed_items_table_));
  db_task_runner_->DeleteSoon(FROM_HERE, std::move(db_));
}

sql::Database* MediaHistoryStoreInternal::DB() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return db_.get();
}

void MediaHistoryStoreInternal::SavePlayback(
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

void MediaHistoryStoreInternal::Initialize() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  db_ = std::make_unique<sql::Database>();
  db_->set_histogram_tag("MediaHistory");

  bool success = db_->Open(db_path_);
  DCHECK(success);

  db_->Preload();

  if (!db_->Execute("PRAGMA foreign_keys=1")) {
    LOG(ERROR) << "Failed to enable foreign keys on the media history store.";
    db_->Poison();

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kFailedNoForeignKeys);

    return;
  }

  meta_table_.Init(db_.get(), GetCurrentVersion(), kCompatibleVersionNumber);
  sql::InitStatus status = CreateOrUpgradeIfNeeded();
  if (status != sql::INIT_OK) {
    LOG(ERROR) << "Failed to create or update the media history store.";
    db_->Poison();

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kFailedDatabaseTooNew);

    return;
  }

  status = InitializeTables();
  if (status != sql::INIT_OK) {
    LOG(ERROR) << "Failed to initialize the media history store tables.";
    db_->Poison();

    base::UmaHistogramEnumeration(
        MediaHistoryStore::kInitResultHistogramName,
        MediaHistoryStore::InitResult::kFailedInitializeTables);

    return;
  }

  initialization_successful_ = true;

  base::UmaHistogramEnumeration(MediaHistoryStore::kInitResultHistogramName,
                                MediaHistoryStore::InitResult::kSuccess);

  // Get the size in bytes.
  int64_t file_size = 0;
  base::GetFileSize(db_path_, &file_size);
  DCHECK_NE(0, file_size);

  // Record the size in KB.
  if (file_size > 0) {
    base::UmaHistogramMemoryKB(MediaHistoryStore::kDatabaseSizeKbHistogramName,
                               file_size / 1000);
  }
}

sql::InitStatus MediaHistoryStoreInternal::CreateOrUpgradeIfNeeded() {
  if (!db_)
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

sql::InitStatus MediaHistoryStoreInternal::InitializeTables() {
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

bool MediaHistoryStoreInternal::CreateOriginId(const url::Origin& origin) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return false;

  return origin_table_->CreateOriginId(origin);
}

mojom::MediaHistoryStatsPtr MediaHistoryStoreInternal::GetMediaHistoryStats() {
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
MediaHistoryStoreInternal::GetOriginRowsForDebug() {
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
MediaHistoryStoreInternal::GetMediaHistoryPlaybackRowsForDebug() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return std::vector<mojom::MediaHistoryPlaybackRowPtr>();

  return playback_table_->GetPlaybackRows();
}

std::vector<media_feeds::mojom::MediaFeedPtr>
MediaHistoryStoreInternal::GetMediaFeedsForDebug() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_ || !feeds_table_)
    return std::vector<media_feeds::mojom::MediaFeedPtr>();

  return feeds_table_->GetRows();
}

int MediaHistoryStoreInternal::GetTableRowCount(const std::string& table_name) {
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

void MediaHistoryStoreInternal::SavePlaybackSession(
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
MediaHistoryStoreInternal::GetPlaybackSessions(
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

void MediaHistoryStoreInternal::RazeAndClose() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (db_ && db_->is_open())
    db_->RazeAndClose();

  sql::Database::Delete(db_path_);
}

void MediaHistoryStoreInternal::DeleteAllOriginData(
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

void MediaHistoryStoreInternal::DeleteAllURLData(const std::set<GURL>& urls) {
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

std::set<GURL> MediaHistoryStoreInternal::GetURLsInTableForTest(
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

void MediaHistoryStoreInternal::DiscoverMediaFeed(const GURL& url) {
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

void MediaHistoryStoreInternal::StoreMediaFeedFetchResult(
    const int64_t feed_id,
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items,
    const media_feeds::mojom::FetchResult result,
    const base::Time& expiry_time,
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
          feed_id, result, expiry_time, items.size(), item_play_next_count,
          item_content_types, logos, display_name)) {
    DB()->RollbackTransaction();
    return;
  }

  DB()->CommitTransaction();
}

std::vector<media_feeds::mojom::MediaFeedItemPtr>
MediaHistoryStoreInternal::GetItemsForMediaFeedForDebug(const int64_t feed_id) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (!initialization_successful_ || !feed_items_table_)
    return std::vector<media_feeds::mojom::MediaFeedItemPtr>();

  return feed_items_table_->GetItemsForFeed(feed_id);
}

MediaHistoryKeyedService::PendingSafeSearchCheckList
MediaHistoryStoreInternal::GetPendingSafeSearchCheckMediaFeedItems() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (!initialization_successful_ || !feed_items_table_)
    return MediaHistoryKeyedService::PendingSafeSearchCheckList();

  return feed_items_table_->GetPendingSafeSearchCheckItems();
}

void MediaHistoryStoreInternal::StoreMediaFeedItemSafeSearchResults(
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
    : db_(new MediaHistoryStoreInternal(profile, db_task_runner)),
      profile_(profile) {
  db_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MediaHistoryStoreInternal::Initialize, db_));
}

MediaHistoryStore::~MediaHistoryStore() {}

void MediaHistoryStore::SavePlayback(
    const content::MediaPlayerWatchTime& watch_time) {
  if (!db_->initialization_successful_)
    return;

  db_->db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaHistoryStoreInternal::SavePlayback, db_,
                                watch_time));
}

void MediaHistoryStore::GetPendingSafeSearchCheckMediaFeedItems(
    base::OnceCallback<
        void(MediaHistoryKeyedService::PendingSafeSearchCheckList)> callback) {
  if (!db_->initialization_successful_) {
    return std::move(callback).Run(
        MediaHistoryKeyedService::PendingSafeSearchCheckList());
  }

  base::PostTaskAndReplyWithResult(
      db_->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &MediaHistoryStoreInternal::GetPendingSafeSearchCheckMediaFeedItems,
          db_),
      std::move(callback));
}

void MediaHistoryStore::StoreMediaFeedItemSafeSearchResults(
    std::map<int64_t, media_feeds::mojom::SafeSearchResult> results) {
  if (!db_->initialization_successful_)
    return;

  db_->db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaHistoryStoreInternal::StoreMediaFeedItemSafeSearchResults, db_,
          results));
}

scoped_refptr<base::UpdateableSequencedTaskRunner>
MediaHistoryStore::GetDBTaskRunnerForTest() {
  return db_->db_task_runner_;
}

void MediaHistoryStore::EraseDatabaseAndCreateNew() {
  auto db_task_runner = db_->db_task_runner_;
  auto db_path = db_->db_path_;

  db_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MediaHistoryStoreInternal::RazeAndClose, db_));

  // Create a new internal store.
  db_ = new MediaHistoryStoreInternal(profile_, db_task_runner);
  db_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MediaHistoryStoreInternal::Initialize, db_));
}

void MediaHistoryStore::GetMediaHistoryStats(
    base::OnceCallback<void(mojom::MediaHistoryStatsPtr)> callback) {
  if (!db_->initialization_successful_)
    return std::move(callback).Run(mojom::MediaHistoryStats::New());

  base::PostTaskAndReplyWithResult(
      db_->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStoreInternal::GetMediaHistoryStats, db_),
      std::move(callback));
}

void MediaHistoryStore::GetOriginRowsForDebug(
    base::OnceCallback<void(std::vector<mojom::MediaHistoryOriginRowPtr>)>
        callback) {
  if (!db_->initialization_successful_) {
    return std::move(callback).Run(
        std::vector<mojom::MediaHistoryOriginRowPtr>());
  }

  base::PostTaskAndReplyWithResult(
      db_->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStoreInternal::GetOriginRowsForDebug, db_),
      std::move(callback));
}

void MediaHistoryStore::GetMediaHistoryPlaybackRowsForDebug(
    base::OnceCallback<void(std::vector<mojom::MediaHistoryPlaybackRowPtr>)>
        callback) {
  base::PostTaskAndReplyWithResult(
      db_->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &MediaHistoryStoreInternal::GetMediaHistoryPlaybackRowsForDebug, db_),
      std::move(callback));
}

void MediaHistoryStore::GetMediaFeedsForDebug(
    base::OnceCallback<void(std::vector<media_feeds::mojom::MediaFeedPtr>)>
        callback) {
  base::PostTaskAndReplyWithResult(
      db_->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStoreInternal::GetMediaFeedsForDebug, db_),
      std::move(callback));
}

void MediaHistoryStore::SavePlaybackSession(
    const GURL& url,
    const media_session::MediaMetadata& metadata,
    const base::Optional<media_session::MediaPosition>& position,
    const std::vector<media_session::MediaImage>& artwork) {
  if (!db_->initialization_successful_)
    return;

  db_->db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaHistoryStoreInternal::SavePlaybackSession,
                                db_, url, metadata, position, artwork));
}

void MediaHistoryStore::GetPlaybackSessions(
    base::Optional<unsigned int> num_sessions,
    base::Optional<GetPlaybackSessionsFilter> filter,
    base::OnceCallback<
        void(std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>)> callback) {
  base::PostTaskAndReplyWithResult(
      db_->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStoreInternal::GetPlaybackSessions, db_,
                     num_sessions, std::move(filter)),
      std::move(callback));
}

void MediaHistoryStore::DeleteAllOriginData(
    const std::set<url::Origin>& origins) {
  db_->db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaHistoryStoreInternal::DeleteAllOriginData,
                                db_, origins));
}

void MediaHistoryStore::DeleteAllURLData(const std::set<GURL>& urls) {
  db_->db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaHistoryStoreInternal::DeleteAllURLData, db_, urls));
}

void MediaHistoryStore::GetURLsInTableForTest(
    const std::string& table,
    base::OnceCallback<void(std::set<GURL>)> callback) {
  base::PostTaskAndReplyWithResult(
      db_->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStoreInternal::GetURLsInTableForTest, db_,
                     table),
      std::move(callback));
}

void MediaHistoryStore::DiscoverMediaFeed(const GURL& url) {
  db_->db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaHistoryStoreInternal::DiscoverMediaFeed, db_, url));
}

void MediaHistoryStore::PostTaskToDBForTest(base::OnceClosure callback) {
  db_->db_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         std::move(callback));
}

void MediaHistoryStore::StoreMediaFeedFetchResult(
    const int64_t feed_id,
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items,
    const media_feeds::mojom::FetchResult result,
    const base::Time& expiry_time,
    const std::vector<media_session::MediaImage>& logos,
    const std::string& display_name) {
  db_->db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaHistoryStoreInternal::StoreMediaFeedFetchResult, db_,
                     feed_id, std::move(items), result, expiry_time, logos,
                     display_name));
}

void MediaHistoryStore::GetItemsForMediaFeedForDebug(
    const int64_t feed_id,
    base::OnceCallback<void(std::vector<media_feeds::mojom::MediaFeedItemPtr>)>
        callback) {
  base::PostTaskAndReplyWithResult(
      db_->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStoreInternal::GetItemsForMediaFeedForDebug,
                     db_, feed_id),
      std::move(callback));
}

}  // namespace media_history
