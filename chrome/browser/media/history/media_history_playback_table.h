// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_PLAYBACK_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_PLAYBACK_TABLE_H_

#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"

namespace base {
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace content {
struct MediaPlayerId;
struct MediaPlayerWatchtime;
}  // namespace content

namespace media_history {

class MediaHistoryPlaybackTable : public MediaHistoryTableBase {
 public:
  struct MediaHistoryPlayback {
    MediaHistoryPlayback() = default;

    bool has_audio;
    bool has_video;
    base::TimeDelta watchtime;
    base::TimeDelta timestamp;
  };

  using MediaHistoryPlaybacks = std::vector<MediaHistoryPlayback>;

 private:
  friend class MediaHistoryStoreInternal;

  explicit MediaHistoryPlaybackTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  ~MediaHistoryPlaybackTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  void SavePlayback(const content::MediaPlayerId& id,
                    const content::MediaPlayerWatchtime& watchtime);

  // Returns |num_results| playbacks sorted by time with the
  // newest first.
  MediaHistoryPlaybacks GetRecentPlaybacks(int num_results);

  DISALLOW_COPY_AND_ASSIGN(MediaHistoryPlaybackTable);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_PLAYBACK_TABLE_H_
