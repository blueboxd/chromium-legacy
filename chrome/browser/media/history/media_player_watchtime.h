// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_PLAYER_WATCHTIME_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_PLAYER_WATCHTIME_H_

#include <string>

#include "base/time/time.h"

namespace content {

struct MediaPlayerWatchtime {
  MediaPlayerWatchtime(bool has_audio,
                       bool has_video,
                       base::TimeDelta cumulative_watchtime,
                       base::TimeDelta last_timestamp);
  MediaPlayerWatchtime(const MediaPlayerWatchtime& other);
  ~MediaPlayerWatchtime() = default;

  bool has_audio;
  bool has_video;
  base::TimeDelta cumulative_watchtime;
  base::TimeDelta last_timestamp;
  std::string origin;
  std::string url;
};

}  // namespace content

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_PLAYER_WATCHTIME_H_
