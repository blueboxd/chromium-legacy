// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/tools/certificate_tag.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace tools {

TEST(UpdaterCertificateTagTests, ParseUnixTime) {
  base::Time time;
  EXPECT_TRUE(ParseUnixTime("Mon Jan 1 10:00:00 UTC 2013", &time));
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  time.UTCExplode(&exploded);
  EXPECT_EQ(2013, exploded.year);
  EXPECT_EQ(1, exploded.month);
  EXPECT_EQ(2, exploded.day_of_week);
  EXPECT_EQ(10, exploded.hour);
  EXPECT_EQ(0, exploded.minute);
  EXPECT_EQ(0, exploded.second);

  // Tests parsing an invalid date string.
  EXPECT_FALSE(ParseUnixTime("Mon Jan 101 10:00:00 UTC 2013", &time));
}

}  // namespace tools
}  // namespace updater
