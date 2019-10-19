// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_playback_table.h"

#include "base/strings/stringprintf.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_player_watchtime.h"
#include "content/public/browser/media_player_id.h"
#include "sql/statement.h"

namespace media_history {

MediaHistoryPlaybackTable::MediaHistoryPlaybackTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryPlaybackTable::~MediaHistoryPlaybackTable() = default;

sql::InitStatus MediaHistoryPlaybackTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      "CREATE TABLE IF NOT EXISTS playback("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "origin_id INTEGER NOT NULL,"
      "url TEXT,"
      "timestamp_ms INTEGER,"
      "has_audio INTEGER,"
      "has_video INTEGER,"
      "watchtime_ms INTEGER,"
      "CONSTRAINT fk_origin "
      "FOREIGN KEY (origin_id) "
      "REFERENCES origin(id) "
      "ON DELETE CASCADE"
      ")");

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS origin_id_index ON "
        "playback (origin_id)");
  }

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS timestamp_index ON "
        "playback (timestamp_ms)");
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history playback table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

void MediaHistoryPlaybackTable::SavePlayback(
    const content::MediaPlayerId& id,
    const content::MediaPlayerWatchtime& watchtime) {
  if (!CanAccessDatabase())
    return;

  if (!DB()->BeginTransaction())
    return;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO playback "
      "(origin_id, has_audio, has_video, watchtime_ms, url, timestamp_ms) "
      "VALUES ((SELECT id FROM origin WHERE origin = ?), ?, ?, ?, ?, ?)"));
  statement.BindString(0, watchtime.origin);
  statement.BindBool(1, watchtime.has_audio);
  statement.BindBool(2, watchtime.has_video);
  statement.BindInt(3, watchtime.cumulative_watchtime.InMilliseconds());
  statement.BindString(4, watchtime.url);
  statement.BindInt(5, watchtime.last_timestamp.InMilliseconds());
  if (!statement.Run()) {
    DB()->RollbackTransaction();
    return;
  }

  DB()->CommitTransaction();
}

MediaHistoryPlaybackTable::MediaHistoryPlaybacks
MediaHistoryPlaybackTable::GetRecentPlaybacks(int num_results) {
  MediaHistoryPlaybacks results;

  sql::Statement statement(
      DB()->GetCachedStatement(SQL_FROM_HERE,
                               "SELECT id, origin_id, has_audio, has_video, "
                               "watchtime_ms, timestamp_ms"
                               "FROM playback ORDER BY id DESC"
                               "LIMIT ?"));
  statement.BindInt(0, num_results);

  while (statement.Step()) {
    MediaHistoryPlaybackTable::MediaHistoryPlayback playback;
    playback.has_audio = statement.ColumnInt(2);
    playback.has_video = statement.ColumnInt(3);
    playback.watchtime =
        base::TimeDelta::FromMilliseconds(statement.ColumnInt(4));
    playback.timestamp =
        base::TimeDelta::FromMilliseconds(statement.ColumnInt(5));
    results.push_back(playback);
  }

  return results;
}

}  // namespace media_history
