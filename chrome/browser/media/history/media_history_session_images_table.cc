// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_session_images_table.h"

#include "base/strings/stringprintf.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_images_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "sql/statement.h"

namespace media_history {

const char MediaHistorySessionImagesTable::kTableName[] = "sessionImage";

MediaHistorySessionImagesTable::MediaHistorySessionImagesTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistorySessionImagesTable::~MediaHistorySessionImagesTable() = default;

sql::InitStatus MediaHistorySessionImagesTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      base::StringPrintf("CREATE TABLE IF NOT EXISTS %s("
                         "session_id INTEGER NOT NULL,"
                         "image_id INTEGER NOT NULL,"
                         "width INTEGER,"
                         "height INTEGER, "
                         "CONSTRAINT fk_session "
                         "FOREIGN KEY (session_id) "
                         "REFERENCES %s(id) "
                         "ON DELETE CASCADE, "
                         "CONSTRAINT fk_image "
                         "FOREIGN KEY (image_id) "
                         "REFERENCES %s(id) "
                         "ON DELETE CASCADE "
                         ")",
                         kTableName, MediaHistorySessionTable::kTableName,
                         MediaHistoryImagesTable::kTableName)
          .c_str());

  if (success) {
    success = DB()->Execute(
        base::StringPrintf("CREATE INDEX IF NOT EXISTS session_id_index ON "
                           "%s (session_id)",
                           kTableName)
            .c_str());
  }

  if (success) {
    success = DB()->Execute(
        base::StringPrintf("CREATE INDEX IF NOT EXISTS image_id_index ON "
                           "%s (image_id)",
                           kTableName)
            .c_str());
  }

  if (success) {
    success = DB()->Execute(
        base::StringPrintf(
            "CREATE UNIQUE INDEX IF NOT EXISTS session_image_index ON "
            "%s (session_id, image_id, width, height)",
            kTableName)
            .c_str());
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history session images table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

}  // namespace media_history
