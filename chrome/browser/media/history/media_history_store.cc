// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/media/history/media_history_engagement_table.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_playback_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "content/public/browser/media_player_watch_time.h"
#include "services/media_session/public/cpp/media_position.h"
#include "sql/statement.h"

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

  // Opens the database file from the profile path. Separated from the
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

  void SavePlaybackSession(
      const GURL& url,
      const media_session::MediaMetadata& metadata,
      const base::Optional<media_session::MediaPosition>& position);

  base::Optional<MediaHistoryStore::MediaPlaybackSessionList>
  GetPlaybackSessions(unsigned int num_sessions,
                      MediaHistoryStore::GetPlaybackSessionsFilter filter);

  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;
  base::FilePath db_path_;
  std::unique_ptr<sql::Database> db_;
  sql::MetaTable meta_table_;
  scoped_refptr<MediaHistoryEngagementTable> engagement_table_;
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
      engagement_table_(new MediaHistoryEngagementTable(db_task_runner_)),
      origin_table_(new MediaHistoryOriginTable(db_task_runner_)),
      playback_table_(new MediaHistoryPlaybackTable(db_task_runner_)),
      session_table_(new MediaHistorySessionTable(db_task_runner_)),
      initialization_successful_(false) {}

MediaHistoryStoreInternal::~MediaHistoryStoreInternal() {
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(engagement_table_));
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

  if (CreateOriginId(watch_time.origin.spec()) &&
      playback_table_->SavePlayback(watch_time)) {
    DB()->CommitTransaction();
  } else {
    DB()->RollbackTransaction();
  }
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
  sql::InitStatus status = engagement_table_->Initialize(db_.get());
  if (status == sql::INIT_OK)
    status = origin_table_->Initialize(db_.get());
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

MediaHistoryStore::MediaHistoryStore(
    Profile* profile,
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : db_(new MediaHistoryStoreInternal(profile, db_task_runner)) {
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

void MediaHistoryStore::GetMediaHistoryStats(
    base::OnceCallback<void(mojom::MediaHistoryStatsPtr)> callback) {
  if (!db_->initialization_successful_)
    return std::move(callback).Run(mojom::MediaHistoryStats::New());

  base::PostTaskAndReplyWithResult(
      db_->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStoreInternal::GetMediaHistoryStats, db_),
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
