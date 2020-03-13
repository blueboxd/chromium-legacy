// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_feed_items_table.h"

#include "base/metrics/histogram_functions.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace media_history {

const char MediaHistoryFeedItemsTable::kTableName[] = "mediaFeedItem";

const char MediaHistoryFeedItemsTable::kFeedItemReadResultHistogramName[] =
    "Media.Feeds.FeedItem.ReadResult";

MediaHistoryFeedItemsTable::MediaHistoryFeedItemsTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryFeedItemsTable::~MediaHistoryFeedItemsTable() = default;

sql::InitStatus MediaHistoryFeedItemsTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      "CREATE TABLE IF NOT EXISTS mediaFeedItem("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "feed_id INTEGER NOT NULL,"
      "type INTEGER NOT NULL,"
      "name TEXT, "
      "date_published_s INTEGER,"
      "is_family_friendly INTEGER,"
      "action_status INTEGER NOT NULL,"
      "genre TEXT,"
      "duration_s INTEGER,"
      "is_live INTEGER,"
      "live_start_time_s INTEGER,"
      "live_end_time_s INTEGER,"
      "shown_count INTEGER,"
      "clicked INTEGER, "
      "CONSTRAINT fk_feed "
      "FOREIGN KEY (feed_id) "
      "REFERENCES mediaFeed(id) "
      "ON DELETE CASCADE"
      ")");

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS media_feed_item_feed_id_index ON "
        "mediaFeedItem (feed_id)");
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history feed items table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

bool MediaHistoryFeedItemsTable::SaveItem(
    const int64_t feed_id,
    const media_feeds::mojom::MediaFeedItemPtr& item) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO mediaFeedItem "
      "(feed_id, type, name, date_published_s, is_family_friendly, "
      "action_status, genre, duration_s, is_live, live_start_time_s, "
      "live_end_time_s, shown_count, clicked) VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, feed_id);
  statement.BindInt64(1, static_cast<int>(item->type));
  statement.BindString16(2, item->name);
  statement.BindInt64(
      3, item->date_published.ToDeltaSinceWindowsEpoch().InSeconds());
  statement.BindBool(4, item->is_family_friendly);
  statement.BindInt64(5, static_cast<int>(item->action_status));
  statement.BindString16(6, item->genre);
  statement.BindInt64(7, item->duration.InSeconds());
  statement.BindBool(8, item->is_live);
  statement.BindInt64(
      9, item->live_start_time.ToDeltaSinceWindowsEpoch().InSeconds());
  statement.BindInt64(
      10, item->live_end_time.ToDeltaSinceWindowsEpoch().InSeconds());
  statement.BindInt64(11, item->shown_count);
  statement.BindBool(12, item->clicked);

  return statement.Run();
}

bool MediaHistoryFeedItemsTable::DeleteItems(const int64_t feed_id) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM mediaFeedItem WHERE feed_id = ?"));

  statement.BindInt64(0, feed_id);
  return statement.Run();
}

std::vector<media_feeds::mojom::MediaFeedItemPtr>
MediaHistoryFeedItemsTable::GetItemsForFeed(const int64_t feed_id) {
  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  if (!CanAccessDatabase())
    return items;

  sql::Statement statement(DB()->GetUniqueStatement(
      "SELECT type, name, date_published_s, is_family_friendly, "
      "action_status, genre, duration_s, is_live, live_start_time_s, "
      "live_end_time_s, shown_count, clicked "
      "FROM mediaFeedItem WHERE feed_id = ?"));

  statement.BindInt64(0, feed_id);

  DCHECK(statement.is_valid());

  while (statement.Step()) {
    auto item = media_feeds::mojom::MediaFeedItem::New();

    item->type = static_cast<media_feeds::mojom::MediaFeedItemType>(
        statement.ColumnInt64(0));
    item->action_status =
        static_cast<media_feeds::mojom::MediaFeedItemActionStatus>(
            statement.ColumnInt64(4));

    if (!IsKnownEnumValue(item->type)) {
      base::UmaHistogramEnumeration(kFeedItemReadResultHistogramName,
                                    FeedItemReadResult::kBadType);
      continue;
    }

    if (!IsKnownEnumValue(item->action_status)) {
      base::UmaHistogramEnumeration(kFeedItemReadResultHistogramName,
                                    FeedItemReadResult::kBadActionStatus);
      continue;
    }

    base::UmaHistogramEnumeration(kFeedItemReadResultHistogramName,
                                  FeedItemReadResult::kSuccess);

    item->name = statement.ColumnString16(1);
    item->date_published = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(statement.ColumnInt64(2)));
    item->is_family_friendly = statement.ColumnBool(3);
    item->genre = statement.ColumnString16(5);
    item->duration = base::TimeDelta::FromSeconds(statement.ColumnInt64(6));
    item->is_live = statement.ColumnBool(7);
    item->live_start_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(statement.ColumnInt64(8)));
    item->live_end_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(statement.ColumnInt64(9)));
    item->shown_count = statement.ColumnInt64(10);
    item->clicked = statement.ColumnBool(11);

    items.push_back(std::move(item));
  }

  DCHECK(statement.Succeeded());
  return items;
}

}  // namespace media_history
