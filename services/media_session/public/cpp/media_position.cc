// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_position.h"

namespace media_session {

MediaPosition::MediaPosition() = default;

MediaPosition::MediaPosition(double playback_rate,
                             base::TimeDelta duration,
                             base::TimeDelta position)
    : playback_rate_(playback_rate),
      duration_(duration),
      position_(position),
      last_updated_time_(base::Time::Now()) {
  DCHECK(duration_ >= base::TimeDelta::FromSeconds(0));
  DCHECK(position_ >= base::TimeDelta::FromSeconds(0));
  DCHECK(position_ <= duration_);
}

MediaPosition::~MediaPosition() = default;

base::TimeDelta MediaPosition::duration() const {
  return duration_;
}

base::TimeDelta MediaPosition::GetPosition() const {
  return GetPositionAtTime(base::Time::Now());
}

base::TimeDelta MediaPosition::GetPositionAtTime(base::Time time) const {
  DCHECK(time >= last_updated_time_);

  base::TimeDelta elapsed_time = playback_rate_ * (time - last_updated_time_);
  base::TimeDelta updated_position = position_ + elapsed_time;

  base::TimeDelta start = base::TimeDelta::FromSeconds(0);

  if (updated_position <= start)
    return start;
  else if (updated_position >= duration_)
    return duration_;
  else
    return updated_position;
}

}  // namespace media_session
