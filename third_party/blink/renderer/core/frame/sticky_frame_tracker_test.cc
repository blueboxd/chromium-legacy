// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/core/frame/sticky_frame_tracker.h"

namespace blink {

class StickyFrameTrackerTest : public testing::Test {};

TEST_F(StickyFrameTrackerTest,
       MainFrameScrollOffsetChange_FrameViewportOffsetDoesNotChange_Sticky) {
  StickyFrameTracker tracker;
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 0), IntPoint(0, 0)));
  EXPECT_TRUE(tracker.UpdateStickyStatus(IntPoint(0, 1), IntPoint(0, 0)));
}

TEST_F(
    StickyFrameTrackerTest,
    FrameViewportOffsetRangeTooSmallComparedToMainFrameScrollOffsetRange_Sticky) {
  StickyFrameTracker tracker;
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 0), IntPoint(0, 0)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 1), IntPoint(0, 1)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 2), IntPoint(0, 2)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 3), IntPoint(0, 3)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 4), IntPoint(0, 4)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 5), IntPoint(0, 0)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 6), IntPoint(0, 1)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 7), IntPoint(0, 2)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 8), IntPoint(0, 3)));
  EXPECT_TRUE(tracker.UpdateStickyStatus(IntPoint(0, 9), IntPoint(0, 4)));
}

TEST_F(
    StickyFrameTrackerTest,
    FrameViewportOffsetRangeNotSmallComparedToMainFrameScrollOffsetRange_NotSticky) {
  StickyFrameTracker tracker;
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 0), IntPoint(0, 0)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 1), IntPoint(0, 1)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 2), IntPoint(0, 2)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 3), IntPoint(0, 3)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 4), IntPoint(0, 4)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 5), IntPoint(0, 0)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 6), IntPoint(0, 1)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 7), IntPoint(0, 2)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 8), IntPoint(0, 3)));
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 9), IntPoint(0, 5)));
}

TEST_F(StickyFrameTrackerTest, OnceStickyAlwaysSticky) {
  StickyFrameTracker tracker;
  EXPECT_FALSE(tracker.UpdateStickyStatus(IntPoint(0, 0), IntPoint(0, 0)));
  EXPECT_TRUE(tracker.UpdateStickyStatus(IntPoint(0, 1), IntPoint(0, 0)));

  EXPECT_TRUE(tracker.UpdateStickyStatus(IntPoint(0, 100), IntPoint(0, 100)));
  EXPECT_TRUE(tracker.UpdateStickyStatus(IntPoint(0, 101), IntPoint(0, 101)));
  EXPECT_TRUE(tracker.UpdateStickyStatus(IntPoint(0, 102), IntPoint(0, 102)));
}

TEST_F(StickyFrameTrackerTest, IsLarge) {
  EXPECT_TRUE(StickyFrameTracker::IsLarge(IntSize(10, 10), IntSize(10, 3)));
  EXPECT_FALSE(StickyFrameTracker::IsLarge(IntSize(10, 10), IntSize(10, 2)));
}

}  // namespace blink
