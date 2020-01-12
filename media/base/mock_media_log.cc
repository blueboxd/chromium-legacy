// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_media_log.h"

namespace media {

MockMediaLog::MockMediaLog() = default;

MockMediaLog::~MockMediaLog() = default;

void MockMediaLog::AddLogRecordLocked(std::unique_ptr<MediaLogRecord> event) {
  const auto log_string = MediaEventToLogString(*event);
  VLOG(2) << "MediaLog: " << log_string;
  DoAddLogRecordLogString(log_string);
}

}  // namespace media
