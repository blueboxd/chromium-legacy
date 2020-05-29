// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/offline_item.h"

#include "base/optional.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_items_collection {
namespace {

TEST(OfflineItemTest, OfflineItemSchedule) {
  OfflineItemSchedule schedule(true);
  EXPECT_TRUE(schedule.only_on_wifi);
  schedule = OfflineItemSchedule(false);
  EXPECT_FALSE(schedule.only_on_wifi);

  auto time = base::make_optional(
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromDays(1)));
  schedule = OfflineItemSchedule(time);
  EXPECT_FALSE(schedule.only_on_wifi);
  EXPECT_EQ(schedule.start_time, time);
}

}  // namespace
}  // namespace offline_items_collection
