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
                                       "mime_type TEXT,"
                                       "last_updated_time_s BIGINT NOT NULL)",
                                       kTableName)
                        .c_str());

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history images table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

}  // namespace media_history
