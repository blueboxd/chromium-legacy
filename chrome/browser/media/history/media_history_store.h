// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_STORE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_STORE_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_engagement_table.h"
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
struct MediaMetadata;
struct MediaPosition;
}  // namespace media_session

namespace sql {
class Database;
}

namespace media_history {

class MediaHistoryStoreInternal;

class MediaHistoryStore {
 public:
  MediaHistoryStore(
      Profile* profile,
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  ~MediaHistoryStore();

  // Represents a single playback session stored in the database.
  struct MediaPlaybackSession {
    GURL url;
    base::TimeDelta duration;
    base::TimeDelta position;
    media_session::MediaMetadata metadata;
  };
  using MediaPlaybackSessionList = std::vector<MediaPlaybackSession>;

  // Saves a playback from a single player in the media history store.
  void SavePlayback(const content::MediaPlayerWatchTime& watch_time);

  void GetMediaHistoryStats(
      base::OnceCallback<void(mojom::MediaHistoryStatsPtr)> callback);

  // Gets the playback sessions from the media history store. The results will
  // be ordered by most recent first and be limited to the first |num_sessions|.
  // For each session it calls |filter| and if that returns |true| then that
  // session will be included in the results.
  using GetPlaybackSessionsCallback =
      base::OnceCallback<void(base::Optional<MediaPlaybackSessionList>)>;
  using GetPlaybackSessionsFilter =
      base::RepeatingCallback<bool(const base::TimeDelta& duration,
                                   const base::TimeDelta& position)>;
  void GetPlaybackSessions(unsigned int num_sessions,
                           GetPlaybackSessionsFilter filter,
                           GetPlaybackSessionsCallback callback);

  // Saves a playback session in the media history store.
  void SavePlaybackSession(
      const GURL& url,
      const media_session::MediaMetadata& metadata,
      const base::Optional<media_session::MediaPosition>& position);

 private:
  scoped_refptr<MediaHistoryStoreInternal> db_;
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_STORE_H_
