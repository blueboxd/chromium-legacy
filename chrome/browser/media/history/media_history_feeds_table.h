// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEEDS_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEEDS_TABLE_H_

#include <vector>

#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"
#include "url/gurl.h"

namespace base {
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace media_history {

class MediaHistoryFeedsTable : public MediaHistoryTableBase {
 public:
  static const char kTableName[];

 private:
  friend class MediaHistoryStoreInternal;

  explicit MediaHistoryFeedsTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  MediaHistoryFeedsTable(const MediaHistoryFeedsTable&) = delete;
  MediaHistoryFeedsTable& operator=(const MediaHistoryFeedsTable&) = delete;
  ~MediaHistoryFeedsTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  // Saves a newly discovered feed in the database.
  bool DiscoverFeed(const GURL& url);

  // Returns the feed rows in the database.
  std::vector<media_feeds::mojom::MediaFeedPtr> GetRows();
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEEDS_TABLE_H_
