// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromecast/public/volume_control.h"

#include "base/notreached.h"

namespace chromecast {
namespace media {

std::ostream& operator<<(std::ostream& os, AudioContentType audio_type) {
  switch (audio_type) {
    case AudioContentType::kMedia:
      return os << "MEDIA";
    case AudioContentType::kAlarm:
      return os << "ALARM";
    case AudioContentType::kCommunication:
      return os << "COMMUNICATION";
    case AudioContentType::kOther:
      return os << "OTHER";
    default:
      NOTREACHED()
          << "Add a new entry above, otherwise kNumTypes is not a valid type.";
      return os << "UNKNOWN";
  }
}

std::ostream& operator<<(std::ostream& os,
                         VolumeChangeSource vol_change_source) {
  switch (vol_change_source) {
    case VolumeChangeSource::kUser:
      return os << "USER";
    case VolumeChangeSource::kAutomatic:
      return os << "AUTOMATIC";
    case VolumeChangeSource::kAutoWithFeedback:
      return os << "AUTO_WITH_FEEDBACK";
  }
}

}  // namespace media
}  // namespace chromecast
