// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include "chrome/browser/media/history/media_player_watchtime.h"
#include "content/public/browser/media_player_id.h"

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

  void SavePlayback(const content::MediaPlayerId& id,
                    const content::MediaPlayerWatchtime& watchtime);

 private:
  // Opens the database file from the profile path. Separated from the
  // constructor to ease construction/destruction of this object on one thread
  // and database access on the DB sequence of |db_task_runner_|.
  void Initialize();
  sql::InitStatus CreateOrUpgradeIfNeeded();
  sql::InitStatus InitializeTables();
  void CreateOriginId(const std::string& origin);

  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;
  base::FilePath db_path_;
  std::unique_ptr<sql::Database> db_;
  sql::MetaTable meta_table_;
  scoped_refptr<MediaHistoryEngagementTable> engagement_table_;
  scoped_refptr<MediaHistoryOriginTable> origin_table_;
  scoped_refptr<MediaHistoryPlaybackTable> playback_table_;
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
      initialization_successful_(false) {}

MediaHistoryStoreInternal::~MediaHistoryStoreInternal() {
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(engagement_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(origin_table_));
  db_task_runner_->ReleaseSoon(FROM_HERE, std::move(playback_table_));
  db_task_runner_->DeleteSoon(FROM_HERE, std::move(db_));
}

void MediaHistoryStoreInternal::SavePlayback(
    const content::MediaPlayerId& id,
    const content::MediaPlayerWatchtime& watchtime) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return;

  CreateOriginId(watchtime.origin);
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaHistoryPlaybackTable::SavePlayback,
                                playback_table_, id, watchtime));
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

  return status;
}

void MediaHistoryStoreInternal::CreateOriginId(const std::string& origin) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!initialization_successful_)
    return;

  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaHistoryOriginTable::CreateOriginId,
                                origin_table_, origin));
}

MediaHistoryStore::MediaHistoryStore(
    Profile* profile,
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : db_(new MediaHistoryStoreInternal(profile, db_task_runner)) {
  db_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MediaHistoryStoreInternal::Initialize, db_));
}

MediaHistoryStore::~MediaHistoryStore() {}

}  // namespace media_history
