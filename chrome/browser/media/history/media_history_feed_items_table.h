// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEED_ITEMS_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEED_ITEMS_TABLE_H_

#include <vector>

#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"
#include "url/gurl.h"

namespace base {
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace media_history {

class MediaHistoryFeedItemsTable : public MediaHistoryTableBase {
 public:
  static const char kTableName[];

  static const char kFeedItemReadResultHistogramName[];

  // If we read a feed item from the database then we record the result to
  // |kFeedItemReadResultHistogramName|. Do not change the numbering since this
  // is recorded.
  enum class FeedItemReadResult {
    kSuccess = 0,
    kBadType = 1,
    kBadActionStatus = 2,
    kBadAuthor = 3,
    kBadAction = 4,
    kBadInteractionCounters = 5,
    kBadContentRatings = 6,
    kBadIdentifiers = 7,
    kBadTVEpisode = 8,
    kBadPlayNextCandidate = 9,
    kBadImages = 10,
    kBadSafeSearchResult = 11,
    kBadGenres = 12,
    kMaxValue = kBadGenres,
  };

  MediaHistoryFeedItemsTable(const MediaHistoryFeedItemsTable&) = delete;
  MediaHistoryFeedItemsTable& operator=(const MediaHistoryFeedItemsTable&) =
      delete;

 private:
  friend class MediaHistoryStore;

  explicit MediaHistoryFeedItemsTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  ~MediaHistoryFeedItemsTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  // Saves a newly discovered feed item in the database.
  bool SaveItem(const int64_t feed_id,
                const media_feeds::mojom::MediaFeedItemPtr& item);

  // Deletes all items from a feed.
  bool DeleteItems(const int64_t feed_id);

  // Gets all the items associated with |feed_id|.
  std::vector<media_feeds::mojom::MediaFeedItemPtr> GetItemsForFeed(
      const int64_t feed_id);

  // Returns all the Media Feed Items that have an unknown safe search result.
  MediaHistoryKeyedService::PendingSafeSearchCheckList
  GetPendingSafeSearchCheckItems();

  // Stores the safe search result for |feed_item_id| and returns the ID of the
  // feed if successful.
  base::Optional<int64_t> StoreSafeSearchResult(
      int64_t feed_item_id,
      media_feeds::mojom::SafeSearchResult result);

  // Increments the shown count for the feed item and returns true if
  // successful.
  bool IncrementShownCount(const int64_t feed_item_id);

  // Marks the feed item as clicked and returns true if successful.
  bool MarkAsClicked(const int64_t feed_item_id);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEED_ITEMS_TABLE_H_
