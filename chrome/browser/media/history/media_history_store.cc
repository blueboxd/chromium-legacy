// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_playback_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "content/public/browser/media_player_watch_time.h"
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
  bool CreateOriginId(const std::string& origin);

  void SavePlayback(const content::MediaPlayerWatchTime& watch_time);

  mojom::MediaHistoryStatsPtr GetMediaHistoryStats();
  int GetTableRowCount(const std::string& table_name);

  std::vector<mojom::MediaHistoryOriginRowPtr> GetOriginRowsForDebug();

  std::vector<mojom::MediaHistoryPlaybackRowPtr>
  GetMediaHistoryPlaybackRowsForDebug();

  void SavePlaybackSession(
      const GURL& url,
      const media_session::MediaMetadata& metadata,
      const base::Optional<media_session::MediaPosition>& position);

  base::Optional<MediaHistoryStore::MediaPlaybackSessionList>
  GetPlaybackSessions(unsigned int num_sessions,
                      MediaHistoryStore::GetPlaybackSessionsFilter filter);

  void RazeAndClose();

  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;
  const base::FilePath db_path_;
  std::unique_ptr<sql::Database> db_;
  sql::MetaTable meta_table_;
  scoped_refptr<MediaHistoryOriginTable> origin_table_;
  scoped_refptr<MediaHistoryPlaybackTable> playback_table_;
  scoped_refptr<MediaHistorySessionTable> session_table_;
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
      initialization_successful_(false) {}

MediaHistoryStoreInternal::~MediaHistoryStoreInternal() {
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(origin_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(playback_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(session_table_));
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
    return;
  }

  auto origin = watch_time.origin.spec();

  if (!(CreateOriginId(origin) && playback_table_->SavePlayback(watch_time))) {
    DB()->RollbackTransaction();
    return;
  }

  if (watch_time.has_audio && watch_time.has_video) {
    if (!origin_table_->IncrementAggregateAudioVideoWatchTime(
            origin, watch_time.cumulative_watch_time)) {
      DB()->RollbackTransaction();
      return;
    }
  }

  DB()->CommitTransaction();
}

void MediaHistoryStoreInternal::Initialize() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  db_ = std::make_unique<sql::Database>();
  db_->set_histogram_tag("MediaHistory");

  bool success = db_->Open(db_path_);
  DCHECK(success);

  db_->Preload();

  meta_table_.Init(db_.get(), GetCurrentVersion(), kCompatibleVersionNumber);
  sql::InitStatus status = CreateOrUpgradeIfNeeded();
  if (status != sql::INIT_OK) {
    LOG(ERROR) << "Failed to create or update the media history store.";
    return;
  }

  status = InitializeTables();
  if (status != sql::INIT_OK) {
    LOG(ERROR) << "Failed to initialize the media history store tables.";
    return;
  }

  initialization_successful_ = true;
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

  return status;
}

bool MediaHistoryStoreInternal::CreateOriginId(const std::string& origin) {
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
    const base::Optional<media_session::MediaPosition>& position) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  auto origin = url::Origin::Create(url);
  if (CreateOriginId(origin.Serialize()) &&
      session_table_->SavePlaybackSession(url, origin, metadata, position)) {
    DB()->CommitTransaction();
  } else {
    DB()->RollbackTransaction();
  }
}

base::Optional<MediaHistoryStore::MediaPlaybackSessionList>
MediaHistoryStoreInternal::GetPlaybackSessions(
    unsigned int num_sessions,
    MediaHistoryStore::GetPlaybackSessionsFilter filter) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return base::nullopt;

  return session_table_->GetPlaybackSessions(num_sessions, std::move(filter));
}

void MediaHistoryStoreInternal::RazeAndClose() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (db_ && db_->is_open())
    db_->RazeAndClose();

  sql::Database::Delete(db_path_);
}

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

void MediaHistoryStore::SavePlaybackSession(
    const GURL& url,
    const media_session::MediaMetadata& metadata,
    const base::Optional<media_session::MediaPosition>& position) {
  if (!db_->initialization_successful_)
    return;

  db_->db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaHistoryStoreInternal::SavePlaybackSession,
                                db_, url, metadata, position));
}

void MediaHistoryStore::GetPlaybackSessions(
    unsigned int num_sessions,
    GetPlaybackSessionsFilter filter,
    GetPlaybackSessionsCallback callback) {
  if (!db_->initialization_successful_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  base::PostTaskAndReplyWithResult(
      db_->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStoreInternal::GetPlaybackSessions, db_,
                     num_sessions, std::move(filter)),
      std::move(callback));
}

}  // namespace media_history
