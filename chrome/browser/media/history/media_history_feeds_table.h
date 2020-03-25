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

  static const char kFeedReadResultHistogramName[];

  // If we read a feed item from the database then we record the result to
  // |kFeedReadResultHistogramName|. Do not change the numbering since this
  // is recorded.
  enum class FeedReadResult {
    kSuccess = 0,
    kBadUserStatus = 1,
    kBadFetchResult = 2,
    kBadLogo = 3,
    kBadUserIdentifier = 4,
    kMaxValue = kBadUserIdentifier,
  };

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

  // Updates the feed following a fetch.
  bool UpdateFeedFromFetch(const int64_t feed_id,
                           const media_feeds::mojom::FetchResult result,
                           const base::Time& expiry_time,
                           const int item_count,
                           const int item_play_next_count,
                           const int item_content_types,
                           const std::vector<media_session::MediaImage>& logos,
                           const std::string& display_name);

  // Returns the feed rows in the database.
  std::vector<media_feeds::mojom::MediaFeedPtr> GetRows();
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEEDS_TABLE_H_
