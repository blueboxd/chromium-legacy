// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEED_ASSOCIATED_ORIGINS_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEED_ASSOCIATED_ORIGINS_TABLE_H_

#include <vector>

#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"
#include "url/origin.h"

namespace base {
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace media_history {

class MediaHistoryFeedAssociatedOriginsTable : public MediaHistoryTableBase {
 public:
  MediaHistoryFeedAssociatedOriginsTable(
      const MediaHistoryFeedAssociatedOriginsTable&) = delete;
  MediaHistoryFeedAssociatedOriginsTable& operator=(
      const MediaHistoryFeedAssociatedOriginsTable&) = delete;

 private:
  friend class MediaHistoryStore;

  explicit MediaHistoryFeedAssociatedOriginsTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  ~MediaHistoryFeedAssociatedOriginsTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  // Adds the associated origin and returns whether it was successful.
  bool Add(const url::Origin& origin, const int64_t feed_id);

  // Clears the associated origins for a feed and returns whether it was
  // successful.
  bool Clear(const int64_t feed_id);

  // Gets all the associated origins associated with a feed.
  std::vector<url::Origin> Get(const int64_t feed_id);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEED_ASSOCIATED_ORIGINS_TABLE_H_
