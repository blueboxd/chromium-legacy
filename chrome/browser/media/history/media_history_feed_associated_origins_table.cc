// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_feed_associated_origins_table.h"

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
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
    const url::Origin& origin,
    const bool include_subdomains) {
  std::set<int64_t> feeds;
  if (!CanAccessDatabase())
    return feeds;

  std::vector<std::string> sql;
  sql.push_back("SELECT feed_id, origin FROM mediaFeedAssociatedOrigin");

  sql::Statement statement;
  if (include_subdomains) {
    sql.push_back("WHERE origin LIKE ? OR origin = ?");

    // The origin will be in the format https://example.com.
    std::vector<std::string> wildcard_parts = base::SplitString(
        MediaHistoryOriginTable::GetOriginForStorage(origin),
        url::kStandardSchemeSeparator,
        base::WhitespaceHandling::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (wildcard_parts.size() != 2)
      return feeds;

    statement.Assign(DB()->GetCachedStatement(
        SQL_FROM_HERE, base::JoinString(sql, " ").c_str()));

    statement.BindString(
        0, base::StrCat({wildcard_parts[0], url::kStandardSchemeSeparator, "%.",
                         wildcard_parts[1]}));
    statement.BindString(1,
                         MediaHistoryOriginTable::GetOriginForStorage(origin));
  } else {
    sql.push_back("WHERE origin = ?");

    statement.Assign(DB()->GetCachedStatement(
        SQL_FROM_HERE, base::JoinString(sql, " ").c_str()));

    statement.BindString(0,
                         MediaHistoryOriginTable::GetOriginForStorage(origin));
  }

  while (statement.Step()) {
    if (include_subdomains) {
      // This shouldn't happen but is a backup so we don't accidentally reset
      // feeds that we should not.
      auto url = GURL(statement.ColumnString(1));
      if (!url.DomainIs(origin.host()))
        continue;
    }

    feeds.insert(statement.ColumnInt64(0));
  }

  return feeds;
}

}  // namespace media_history
