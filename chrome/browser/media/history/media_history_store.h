// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_STORE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_STORE_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_playback_table.h"
#include "chrome/browser/media/history/media_history_store.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/media_player_watch_time.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

namespace media_session {
struct MediaImage;
struct MediaMetadata;
struct MediaPosition;
}  // namespace media_session

namespace sql {
class Database;
}  // namespace sql

namespace url {
class Origin;
}  // namespace url

namespace media_history {

class MediaHistoryStoreInternal;

class MediaHistoryStore {
 public:
  MediaHistoryStore(
      Profile* profile,
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  ~MediaHistoryStore();
  MediaHistoryStore(const MediaHistoryStore& t) = delete;
  MediaHistoryStore& operator=(const MediaHistoryStore&) = delete;

  using GetPlaybackSessionsFilter =
      base::RepeatingCallback<bool(const base::TimeDelta& duration,
                                   const base::TimeDelta& position)>;

  static const char kInitResultHistogramName[];
  static const char kPlaybackWriteResultHistogramName[];
  static const char kSessionWriteResultHistogramName[];
  static const char kDatabaseSizeKbHistogramName[];

  // When we initialize the database we store the result in
  // |kInitResultHistogramName|. Do not change the numbering since this
  // is recorded.
  enum class InitResult {
    kSuccess = 0,
    kFailedNoForeignKeys = 1,
    kFailedDatabaseTooNew = 2,
    kFailedInitializeTables = 3,
    kMaxValue = kFailedInitializeTables,
  };

  // If we write a playback into the database then we record the result to
  // |kPlaybackWriteResultHistogramName|. Do not change the numbering since this
  // is recorded.
  enum class PlaybackWriteResult {
    kSuccess = 0,
    kFailedToEstablishTransaction = 1,
    kFailedToWriteOrigin = 2,
    kFailedToWritePlayback = 3,
    kFailedToIncrementAggreatedWatchtime = 4,
    kMaxValue = kFailedToIncrementAggreatedWatchtime,
  };

  // If we write a session into the database then we record the result to
  // |kSessionWriteResultHistogramName|. Do not change the numbering since this
  // is recorded.
  enum class SessionWriteResult {
    kSuccess = 0,
    kFailedToEstablishTransaction = 1,
    kFailedToWriteOrigin = 2,
    kFailedToWriteSession = 3,
    kFailedToWriteImage = 4,
    kMaxValue = kFailedToWriteImage,
  };

 protected:
  friend class MediaHistoryKeyedService;

  // Saves a playback from a single player in the media history store.
  void SavePlayback(const content::MediaPlayerWatchTime& watch_time);

  void GetMediaHistoryStats(
      base::OnceCallback<void(mojom::MediaHistoryStatsPtr)> callback);

  // Returns all the rows in the origin table. This should only be used for
  // debugging because it is very slow.
  void GetOriginRowsForDebug(
      base::OnceCallback<void(std::vector<mojom::MediaHistoryOriginRowPtr>)>
          callback);

  // Returns all the rows in the playback table. This is only used for
  // debugging because it loads all rows in the table.
  void GetMediaHistoryPlaybackRowsForDebug(
      base::OnceCallback<void(std::vector<mojom::MediaHistoryPlaybackRowPtr>)>
          callback);

  // Returns all the rows in the media feeds table.  This is only used for
  // debugging because it loads all rows in the table.
  void GetMediaFeedsForDebug(
      base::OnceCallback<void(std::vector<media_feeds::mojom::MediaFeedPtr>)>
          callback);

  // Gets the playback sessions from the media history store. The results will
  // be ordered by most recent first and be limited to the first |num_sessions|.
  // For each session it calls |filter| and if that returns |true| then that
  // session will be included in the results.
  void GetPlaybackSessions(
      base::Optional<unsigned int> num_sessions,
      base::Optional<GetPlaybackSessionsFilter> filter,
      base::OnceCallback<void(
          std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>)> callback);

  // Saves a playback session in the media history store.
  void SavePlaybackSession(
      const GURL& url,
      const media_session::MediaMetadata& metadata,
      const base::Optional<media_session::MediaPosition>& position,
      const std::vector<media_session::MediaImage>& artwork);

  // Saves a newly discovered media feed in the media history store.
  void DiscoverMediaFeed(const GURL& url);

  void StoreMediaFeedFetchResult(
      const int64_t feed_id,
      std::vector<media_feeds::mojom::MediaFeedItemPtr> items,
      const media_feeds::mojom::FetchResult result,
      const bool was_fetched_from_cache,
      const std::vector<media_session::MediaImage>& logos,
      const std::string& display_name);

  // Gets all the feed items for |feed_id|.
  void GetItemsForMediaFeedForDebug(
      const int64_t feed_id,
      base::OnceCallback<
          void(std::vector<media_feeds::mojom::MediaFeedItemPtr>)> callback);

  void GetPendingSafeSearchCheckMediaFeedItems(
      base::OnceCallback<
          void(MediaHistoryKeyedService::PendingSafeSearchCheckList)> callback);

  void StoreMediaFeedItemSafeSearchResults(
      std::map<int64_t, media_feeds::mojom::SafeSearchResult> results);

  scoped_refptr<base::UpdateableSequencedTaskRunner> GetDBTaskRunnerForTest();

  void GetURLsInTableForTest(const std::string& table,
                             base::OnceCallback<void(std::set<GURL>)> callback);

  void PostTaskToDBForTest(base::OnceClosure callback);
  void EraseDatabaseAndCreateNew();
  void DeleteAllOriginData(const std::set<url::Origin>& origins);
  void DeleteAllURLData(const std::set<GURL>& urls);

 private:
  scoped_refptr<MediaHistoryStoreInternal> db_;

  Profile* const profile_;
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_STORE_H_
