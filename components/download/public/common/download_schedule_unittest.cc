// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_schedule.h"

#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {
namespace {

TEST(DownloadScheduleTest, CtorAndCopy) {
  DownloadSchedule download_schedule(false, base::nullopt);
  EXPECT_FALSE(download_schedule.only_on_wifi());
  EXPECT_EQ(download_schedule.start_time(), base::nullopt);

  download_schedule = DownloadSchedule(true, base::nullopt);
  EXPECT_TRUE(download_schedule.only_on_wifi());
  EXPECT_EQ(download_schedule.start_time(), base::nullopt);

  auto time = base::make_optional(
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromDays(1)));
  download_schedule = DownloadSchedule(false, time);
  EXPECT_FALSE(download_schedule.only_on_wifi());
  EXPECT_EQ(download_schedule.start_time(), time);
}

}  // namespace
}  // namespace download
