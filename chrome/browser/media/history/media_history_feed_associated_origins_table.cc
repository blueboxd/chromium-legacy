// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_feed_associated_origins_table.h"

#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "sql/statement.h"

namespace media_history {

const char MediaHistoryFeedAssociatedOriginsTable::kTableName[] =
    "mediaFeedAssociatedOrigin";

MediaHistoryFeedAssociatedOriginsTable::MediaHistoryFeedAssociatedOriginsTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryFeedAssociatedOriginsTable::
    ~MediaHistoryFeedAssociatedOriginsTable() = default;

sql::InitStatus
MediaHistoryFeedAssociatedOriginsTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      "CREATE TABLE IF NOT EXISTS mediaFeedAssociatedOrigin("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "origin TEXT NOT NULL, "
      "feed_id INTEGER NOT NULL,"
      "CONSTRAINT fk_ao_feed_id "
      "FOREIGN KEY (feed_id) "
      "REFERENCES mediaFeed(id) "
      "ON DELETE CASCADE "
      ")");

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS mediaFeedAssociatedOrigin_feed_index ON "
        "mediaFeedAssociatedOrigin (feed_id)");
  }

  if (success) {
    success = DB()->Execute(
        "CREATE UNIQUE INDEX IF NOT EXISTS "
        "mediaFeedAssociatedOrigin_unique_index ON "
        "mediaFeedAssociatedOrigin(feed_id, origin)");
  }

  if (!success) {
    ResetDB();
    LOG(ERROR)
        << "Failed to create media history feed associated origins table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

bool MediaHistoryFeedAssociatedOriginsTable::Add(const url::Origin& origin,
                                                 const int64_t feed_id) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  DCHECK(feed_id);

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO mediaFeedAssociatedOrigin (origin, feed_id) VALUES (?, ?)"));
  statement.BindString(0, MediaHistoryOriginTable::GetOriginForStorage(origin));
  statement.BindInt64(1, feed_id);
  return statement.Run();
}

bool MediaHistoryFeedAssociatedOriginsTable::Clear(const int64_t feed_id) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  DCHECK(feed_id);

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM mediaFeedAssociatedOrigin WHERE feed_id = ?"));
  statement.BindInt64(0, feed_id);
  return statement.Run();
}

std::vector<url::Origin> MediaHistoryFeedAssociatedOriginsTable::Get(
    const int64_t feed_id) {
  std::vector<url::Origin> origins;
  if (!CanAccessDatabase())
    return origins;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT origin FROM mediaFeedAssociatedOrigin WHERE feed_id = ?"));
  statement.BindInt64(0, feed_id);

  while (statement.Step()) {
    GURL url(statement.ColumnString(0));

    if (!url.is_valid())
      continue;

    origins.push_back(url::Origin::Create(url));
  }

  return origins;
}

std::set<int64_t> MediaHistoryFeedAssociatedOriginsTable::GetFeeds(
    const url::Origin& origin) {
  std::set<int64_t> feeds;
  if (!CanAccessDatabase())
    return feeds;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT feed_id FROM mediaFeedAssociatedOrigin WHERE origin = ?"));
  statement.BindString(0, MediaHistoryOriginTable::GetOriginForStorage(origin));

  while (statement.Step())
    feeds.insert(statement.ColumnInt64(0));

  return feeds;
}

}  // namespace media_history
