// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_feeds_table.h"

#include "base/strings/stringprintf.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace media_history {

const char MediaHistoryFeedsTable::kTableName[] = "mediaFeed";

MediaHistoryFeedsTable::MediaHistoryFeedsTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryFeedsTable::~MediaHistoryFeedsTable() = default;

sql::InitStatus MediaHistoryFeedsTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success =
      DB()->Execute(base::StringPrintf("CREATE TABLE IF NOT EXISTS %s("
                                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                       "origin_id INTEGER NOT NULL UNIQUE,"
                                       "url TEXT NOT NULL, "
                                       "last_discovery_time_s INTEGER, "
                                       "CONSTRAINT fk_origin "
                                       "FOREIGN KEY (origin_id) "
                                       "REFERENCES origin(id) "
                                       "ON DELETE CASCADE"
                                       ")",
                                       kTableName)
                        .c_str());

  if (success) {
    success = DB()->Execute(
        base::StringPrintf(
            "CREATE INDEX IF NOT EXISTS media_feed_origin_id_index ON "
            "%s (origin_id)",
            kTableName)
            .c_str());
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history feeds table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

bool MediaHistoryFeedsTable::DiscoverFeed(const GURL& url) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  const auto origin =
      MediaHistoryOriginTable::GetOriginForStorage(url::Origin::Create(url));
  const auto now = base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  base::Optional<GURL> feed_url;
  base::Optional<int64_t> feed_id;

  {
    // Check if we already have a feed for the current origin;
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT id, url FROM mediaFeed WHERE origin_id = (SELECT id FROM "
        "origin WHERE origin = ?)"));
    statement.BindString(0, origin);

    while (statement.Step()) {
      DCHECK(!feed_id);
      DCHECK(!feed_url);

      feed_id = statement.ColumnInt64(0);
      feed_url = GURL(statement.ColumnString(1));
    }
  }

  if (!feed_url || url != feed_url) {
    // If the feed does not exist or exists and has a different URL then we
    // should replace the feed.
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT OR REPLACE INTO mediaFeed "
        "(origin_id, url, last_discovery_time_s) VALUES "
        "((SELECT id FROM origin WHERE origin = ?), ?, ?)"));
    statement.BindString(0, origin);
    statement.BindString(1, url.spec());
    statement.BindInt64(2, now);
    return statement.Run() && DB()->GetLastChangeCount() == 1;
  } else {
    // If the feed already exists in the database with the same URL we should
    // just update the last discovery time so we don't delete the old entry.
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE mediaFeed SET last_discovery_time_s = ? WHERE id = ?"));
    statement.BindInt64(0, now);
    statement.BindInt64(1, *feed_id);
    return statement.Run() && DB()->GetLastChangeCount() == 1;
  }
}

std::vector<media_feeds::mojom::MediaFeedPtr>
MediaHistoryFeedsTable::GetRows() {
  std::vector<media_feeds::mojom::MediaFeedPtr> feeds;
  if (!CanAccessDatabase())
    return feeds;

  sql::Statement statement(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT id, url, last_discovery_time_s "
                         "FROM %s",
                         kTableName)
          .c_str()));

  while (statement.Step()) {
    media_feeds::mojom::MediaFeedPtr feed(media_feeds::mojom::MediaFeed::New());

    feed->id = statement.ColumnInt64(0);
    feed->url = GURL(statement.ColumnString(1));
    feed->last_discovery_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(statement.ColumnInt64(2)));

    feeds.push_back(std::move(feed));
  }

  DCHECK(statement.Succeeded());
  return feeds;
}

}  // namespace media_history
