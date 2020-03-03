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

bool MediaHistoryFeedsTable::SaveFeed(const GURL& url) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("INSERT OR REPLACE INTO %s "
                         "(origin_id, url) VALUES "
                         "((SELECT id FROM origin WHERE origin = ?), ?)",
                         kTableName)
          .c_str()));
  statement.BindString(0, MediaHistoryOriginTable::GetOriginForStorage(
                              url::Origin::Create(url)));
  statement.BindString(1, url.spec());

  return statement.Run();
}

}  // namespace media_history
