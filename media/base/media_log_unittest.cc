// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "base/macros.h"
#include "media/base/media_log.h"
#include "media/base/mock_media_log.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media {

// Friend class of MediaLog for access to internal constants.
class MediaLogTest : public testing::Test {
 public:
  static constexpr size_t kMaxUrlLength = MediaLog::kMaxUrlLength;

 protected:
  MediaLog media_log;
};

constexpr size_t MediaLogTest::kMaxUrlLength;


TEST_F(MediaLogTest, EventsAreForwarded) {
  // Make sure that |root_log_| receives events.
  std::unique_ptr<MockMediaLog> root_log(std::make_unique<MockMediaLog>());
  std::unique_ptr<MediaLog> child_media_log(root_log->Clone());
  EXPECT_CALL(*root_log, DoAddLogRecordLogString(_)).Times(1);
  child_media_log->AddMessage(MediaLogMessageLevel::kERROR, "test");
}

TEST_F(MediaLogTest, EventsAreNotForwardedAfterInvalidate) {
  // Make sure that |root_log_| doesn't forward things after we invalidate the
  // underlying log.
  std::unique_ptr<MockMediaLog> root_log(std::make_unique<MockMediaLog>());
  std::unique_ptr<MediaLog> child_media_log(root_log->Clone());
  EXPECT_CALL(*root_log, DoAddLogRecordLogString(_)).Times(0);
  root_log.reset();
  child_media_log->AddMessage(MediaLogMessageLevel::kERROR, "test");
}

TEST_F(MediaLogTest, ClonedLogsInhertParentPlayerId) {
  std::unique_ptr<MockMediaLog> root_log(std::make_unique<MockMediaLog>());
  std::unique_ptr<MediaLog> child_media_log(root_log->Clone());
  child_media_log->AddMessage(MediaLogMessageLevel::kERROR, "test");
  auto event = root_log->take_most_recent_event();
  EXPECT_NE(event, nullptr);
  EXPECT_EQ(event->id, root_log->id());
}

}  // namespace media
