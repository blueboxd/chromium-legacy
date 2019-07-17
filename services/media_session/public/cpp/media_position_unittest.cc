// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_position.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

class MediaPositionTest : public testing::Test {};

TEST_F(MediaPositionTest, TestPositionUpdated) {
  MediaPosition media_position(
      1 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  base::Time now = base::Time::Now() + base::TimeDelta::FromSeconds(100);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  EXPECT_EQ(updated_position.InSeconds(), 400);
}

TEST_F(MediaPositionTest, TestPositionUpdatedTwice) {
  MediaPosition media_position(
      1 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(200) /* position */);

  base::Time now = base::Time::Now() + base::TimeDelta::FromSeconds(100);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  EXPECT_EQ(updated_position.InSeconds(), 300);

  now += base::TimeDelta::FromSeconds(100);
  updated_position = media_position.GetPositionAtTime(now);

  EXPECT_EQ(updated_position.InSeconds(), 400);
}

TEST_F(MediaPositionTest, TestPositionUpdatedPastDuration) {
  MediaPosition media_position(
      1 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  base::Time now = base::Time::Now() + base::TimeDelta::FromSeconds(400);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  // Verify that the position has been updated to the end of the total duration.
  EXPECT_EQ(updated_position.InSeconds(), 600);
}

TEST_F(MediaPositionTest, TestPositionAtStart) {
  MediaPosition media_position(1 /* playback_rate */,
                               base::TimeDelta::FromSeconds(600) /* duration */,
                               base::TimeDelta::FromSeconds(0) /* position */);

  base::TimeDelta updated_position = media_position.GetPosition();

  EXPECT_EQ(updated_position.InSeconds(), 0);
}

TEST_F(MediaPositionTest, TestNegativePosition) {
  MediaPosition media_position(
      -1 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  base::Time now = base::Time::Now() + base::TimeDelta::FromSeconds(400);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  // Verify that the position does not go below 0.
  EXPECT_EQ(updated_position.InSeconds(), 0);
}

TEST_F(MediaPositionTest, TestPositionUpdatedNoChange) {
  MediaPosition media_position(
      1 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  // Get the updated position without moving forward in time.
  base::TimeDelta updated_position = media_position.GetPosition();

  // Verify that the updated position has not changed.
  EXPECT_EQ(updated_position.InSeconds(), 300);
}

TEST_F(MediaPositionTest, TestPositionUpdatedFasterPlayback) {
  MediaPosition media_position(
      2 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  base::Time now = base::Time::Now() + base::TimeDelta::FromSeconds(100);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  EXPECT_EQ(updated_position.InSeconds(), 500);
}

TEST_F(MediaPositionTest, TestPositionUpdatedSlowerPlayback) {
  MediaPosition media_position(
      .5 /* playback_rate */, base::TimeDelta::FromSeconds(600) /* duration */,
      base::TimeDelta::FromSeconds(300) /* position */);

  base::Time now = base::Time::Now() + base::TimeDelta::FromSeconds(200);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  EXPECT_EQ(updated_position.InSeconds(), 400);
}
}  // namespace media_session