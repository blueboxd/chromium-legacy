// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_player_watchtime.h"

namespace content {

MediaPlayerWatchtime::MediaPlayerWatchtime(bool has_audio,
                                           bool has_video,
                                           base::TimeDelta cumulative_watchtime,
                                           base::TimeDelta last_timestamp)
    : has_audio(has_audio),
      has_video(has_video),
      cumulative_watchtime(cumulative_watchtime),
      last_timestamp(last_timestamp) {}

MediaPlayerWatchtime::MediaPlayerWatchtime(const MediaPlayerWatchtime& other) =
    default;

}  // namespace content
