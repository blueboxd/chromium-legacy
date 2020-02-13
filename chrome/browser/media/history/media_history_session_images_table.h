// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_SESSION_IMAGES_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_SESSION_IMAGES_TABLE_H_

#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"

namespace base {
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace media_history {

class MediaHistorySessionImagesTable : public MediaHistoryTableBase {
 public:
  static const char kTableName[];

 private:
  friend class MediaHistoryStoreInternal;

  explicit MediaHistorySessionImagesTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  MediaHistorySessionImagesTable(const MediaHistorySessionImagesTable&) =
      delete;
  MediaHistorySessionImagesTable& operator=(
      const MediaHistorySessionImagesTable&) = delete;
  ~MediaHistorySessionImagesTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_SESSION_IMAGES_TABLE_H_
