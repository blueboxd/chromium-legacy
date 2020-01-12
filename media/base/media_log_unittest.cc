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

TEST_F(MediaLogTest, DontTruncateShortUrlString) {
  const std::string short_url("chromium.org");
  EXPECT_LT(short_url.length(), MediaLogTest::kMaxUrlLength);

  // Verify that CreatedEvent does not truncate the short URL.
  std::unique_ptr<MediaLogRecord> created_event =
      media_log.CreateCreatedEvent(short_url);
  std::string stored_url;
  created_event->params.GetString("origin_url", &stored_url);
  EXPECT_EQ(stored_url, short_url);

  // Verify that LoadEvent does not truncate the short URL.
  std::unique_ptr<MediaLogRecord> load_event =
      media_log.CreateLoadEvent(short_url);
  load_event->params.GetString("url", &stored_url);
  EXPECT_EQ(stored_url, short_url);
}

TEST_F(MediaLogTest, TruncateLongUrlStrings) {
  // Build a long string that exceeds the URL length limit.
  std::stringstream string_builder;
  constexpr size_t kLongStringLength = MediaLogTest::kMaxUrlLength + 10;
  for (size_t i = 0; i < kLongStringLength; i++) {
    string_builder << "c";
  }
  const std::string long_url = string_builder.str();
  EXPECT_GT(long_url.length(), MediaLogTest::kMaxUrlLength);

  // Verify that long CreatedEvent URL...
  std::unique_ptr<MediaLogRecord> created_event =
      media_log.CreateCreatedEvent(long_url);
  std::string stored_url;
  created_event->params.GetString("origin_url", &stored_url);

  // ... is truncated
  EXPECT_EQ(stored_url.length(), MediaLogTest::kMaxUrlLength);
  // ... ends with ellipsis
  EXPECT_EQ(stored_url.compare(MediaLogTest::kMaxUrlLength - 3, 3, "..."), 0);
  // ... is otherwise a substring of the longer URL
  EXPECT_EQ(stored_url.compare(0, MediaLogTest::kMaxUrlLength - 3, long_url, 0,
                               MediaLogTest::kMaxUrlLength - 3),
            0);

  // Verify that long LoadEvent URL...
  std::unique_ptr<MediaLogRecord> load_event =
      media_log.CreateCreatedEvent(long_url);
  load_event->params.GetString("url", &stored_url);
  // ... is truncated
  EXPECT_EQ(stored_url.length(), MediaLogTest::kMaxUrlLength);
  // ... ends with ellipsis
  EXPECT_EQ(stored_url.compare(MediaLogTest::kMaxUrlLength - 3, 3, "..."), 0);
  // ... is otherwise a substring of the longer URL
  EXPECT_EQ(stored_url.compare(0, MediaLogTest::kMaxUrlLength - 3, long_url, 0,
                               MediaLogTest::kMaxUrlLength - 3),
            0);
}

TEST_F(MediaLogTest, EventsAreForwarded) {
  // Make sure that |root_log_| receives events.
  std::unique_ptr<MockMediaLog> root_log(std::make_unique<MockMediaLog>());
  std::unique_ptr<MediaLog> child_media_log(root_log->Clone());
  EXPECT_CALL(*root_log, DoAddLogRecordLogString(_)).Times(1);
  child_media_log->AddLogEvent(MediaLog::MediaLogLevel::MEDIALOG_ERROR, "test");
}

TEST_F(MediaLogTest, EventsAreNotForwardedAfterInvalidate) {
  // Make sure that |root_log_| doesn't forward things after we invalidate the
  // underlying log.
  std::unique_ptr<MockMediaLog> root_log(std::make_unique<MockMediaLog>());
  std::unique_ptr<MediaLog> child_media_log(root_log->Clone());
  EXPECT_CALL(*root_log, DoAddLogRecordLogString(_)).Times(0);
  root_log.reset();
  child_media_log->AddLogEvent(MediaLog::MediaLogLevel::MEDIALOG_ERROR, "test");
}

}  // namespace media
