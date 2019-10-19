// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_origin_table.h"

#include "base/strings/stringprintf.h"
#include "sql/statement.h"

namespace media_history {

MediaHistoryOriginTable::MediaHistoryOriginTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryOriginTable::~MediaHistoryOriginTable() = default;

sql::InitStatus MediaHistoryOriginTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      "CREATE TABLE IF NOT EXISTS origin("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "origin TEXT NOT NULL UNIQUE)");

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history origin table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

void MediaHistoryOriginTable::CreateOriginId(const std::string& origin) {
  if (!CanAccessDatabase())
    return;

  if (!DB()->BeginTransaction()) {
    LOG(ERROR) << "Failed to begin the transaction to create an origin ID.";
    return;
  }

  // Insert the origin into the table if it does not exist.
  sql::Statement statement(
      DB()->GetCachedStatement(SQL_FROM_HERE,
                               "INSERT OR IGNORE INTO origin"
                               "(origin) "
                               "VALUES (?)"));
  statement.BindString(0, origin);
  if (!statement.Run()) {
    DB()->RollbackTransaction();
    LOG(ERROR) << "Failed to create the origin ID.";
    return;
  }

  DB()->CommitTransaction();
}

}  // namespace media_history
