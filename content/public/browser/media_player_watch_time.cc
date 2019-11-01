// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_player_watch_time.h"

namespace content {

MediaPlayerWatchTime::MediaPlayerWatchTime(
    GURL url,
    GURL origin,
    base::TimeDelta cumulative_watch_time,
    base::TimeDelta last_timestamp)
    : url(url),
      origin(origin),
      cumulative_watch_time(cumulative_watch_time),
      last_timestamp(last_timestamp) {}

MediaPlayerWatchTime::MediaPlayerWatchTime(const MediaPlayerWatchTime& other) =
    default;

}  // namespace content
