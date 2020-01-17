// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_session_table.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/media_position.h"
#include "sql/statement.h"
#include "url/origin.h"

namespace media_history {

const char MediaHistorySessionTable::kTableName[] = "playbackSession";

MediaHistorySessionTable::MediaHistorySessionTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistorySessionTable::~MediaHistorySessionTable() = default;

sql::InitStatus MediaHistorySessionTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success =
      DB()->Execute(base::StringPrintf("CREATE TABLE IF NOT EXISTS %s("
                                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                       "origin_id INTEGER NOT NULL,"
                                       "url TEXT,"
                                       "duration_ms INTEGER,"
                                       "position_ms INTEGER,"
                                       "last_updated_time_s BIGINT NOT NULL,"
                                       "title TEXT, "
                                       "artist TEXT, "
                                       "album TEXT, "
                                       "source_title TEXT, "
                                       "CONSTRAINT fk_origin "
                                       "FOREIGN KEY (origin_id) "
                                       "REFERENCES origin(id) "
                                       "ON DELETE CASCADE"
                                       ")",
                                       kTableName)
                        .c_str());

  if (success) {
    success = DB()->Execute(
        base::StringPrintf("CREATE INDEX IF NOT EXISTS origin_id_index ON "
                           "%s (origin_id)",
                           kTableName)
            .c_str());
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history playback session table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

bool MediaHistorySessionTable::SavePlaybackSession(
    const GURL& url,
    const url::Origin& origin,
    const media_session::MediaMetadata& metadata,
    const base::Optional<media_session::MediaPosition>& position) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "INSERT INTO %s "
          "(origin_id, url, duration_ms, position_ms, last_updated_time_s, "
          "title, artist, album, source_title) "
          "VALUES "
          "((SELECT id FROM origin WHERE origin = ?), ?, ?, ?, ?, ?, ?, ?, ?)",
          kTableName)
          .c_str()));
  statement.BindString(0, origin.Serialize());
  statement.BindString(1, url.spec());

  if (position.has_value()) {
    auto duration_ms = position->duration().InMilliseconds();
    auto position_ms = position->GetPosition().InMilliseconds();

    statement.BindInt64(2, duration_ms);
    statement.BindInt64(3, position_ms);
  } else {
    statement.BindInt64(2, 0);
    statement.BindInt64(3, 0);
  }

  statement.BindInt64(4,
                      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  statement.BindString16(5, metadata.title);
  statement.BindString16(6, metadata.artist);
  statement.BindString16(7, metadata.album);
  statement.BindString16(8, metadata.source_title);

  return statement.Run();
}

base::Optional<MediaHistoryStore::MediaPlaybackSessionList>
MediaHistorySessionTable::GetPlaybackSessions(
    unsigned int num_sessions,
    MediaHistoryStore::GetPlaybackSessionsFilter filter) {
  if (!CanAccessDatabase())
    return base::nullopt;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("SELECT url, duration_ms, position_ms, title, artist, "
                         "album, source_title FROM %s ORDER BY id DESC",
                         kTableName)
          .c_str()));

  std::set<GURL> previous_urls;
  MediaHistoryStore::MediaPlaybackSessionList sessions;

  while (statement.Step()) {
    // Check if we already have a playback session for this url.
    GURL url(statement.ColumnString(0));
    if (base::Contains(previous_urls, url))
      continue;
    previous_urls.insert(url);

    auto duration = base::TimeDelta::FromMilliseconds(statement.ColumnInt64(1));
    auto position = base::TimeDelta::FromMilliseconds(statement.ColumnInt64(2));

    // Skip any that should not be shown.
    if (!filter.Run(duration, position))
      continue;

    MediaHistoryStore::MediaPlaybackSession session;
    session.url = url;
    session.duration = duration;
    session.position = position;
    session.metadata.title = statement.ColumnString16(3);
    session.metadata.artist = statement.ColumnString16(4);
    session.metadata.album = statement.ColumnString16(5);
    session.metadata.source_title = statement.ColumnString16(6);

    sessions.push_back(std::move(session));

    // If we have all the sessions we want we can stop loading data from the
    // database.
    if (sessions.size() >= num_sessions)
      break;
  }

  return sessions;
}

}  // namespace media_history
