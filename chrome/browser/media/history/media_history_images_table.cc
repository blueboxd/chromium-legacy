// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_images_table.h"

#include "base/strings/stringprintf.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "sql/statement.h"

namespace media_history {

const char MediaHistoryImagesTable::kTableName[] = "mediaImage";

MediaHistoryImagesTable::MediaHistoryImagesTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryImagesTable::~MediaHistoryImagesTable() = default;

sql::InitStatus MediaHistoryImagesTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success =
      DB()->Execute(base::StringPrintf("CREATE TABLE IF NOT EXISTS %s("
                                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                       "url TEXT NOT NULL UNIQUE,"
                                       "mime_type TEXT)",
                                       kTableName)
                        .c_str());

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history images table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

base::Optional<int64_t> MediaHistoryImagesTable::SaveOrGetImage(
    const GURL& url,
    const base::string16& mime_type) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return base::nullopt;

  {
    // First we should try and save the image in the database. It will not save
    // if we already have this image in the DB.
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE, base::StringPrintf("INSERT OR IGNORE INTO %s "
                                          "(url, mime_type) VALUES (?, ?)",
                                          kTableName)
                           .c_str()));
    statement.BindString(0, url.spec());
    statement.BindString16(1, mime_type);

    if (!statement.Run())
      return base::nullopt;
  }

  // If the insert is successful and we have store an image row then we should
  // return the last insert id.
  if (DB()->GetLastChangeCount() == 1) {
    auto id = DB()->GetLastInsertRowId();
    if (id)
      return id;

    NOTREACHED();
  }

  DCHECK_EQ(0, DB()->GetLastChangeCount());

  {
    // If we did not save the image then we need to find the ID of the image.
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        base::StringPrintf("SELECT id FROM %s WHERE url = ?", kTableName)
            .c_str()));
    statement.BindString(0, url.spec());

    while (statement.Step()) {
      return statement.ColumnInt64(0);
    }
  }

  NOTREACHED();
  return base::nullopt;
}

}  // namespace media_history
