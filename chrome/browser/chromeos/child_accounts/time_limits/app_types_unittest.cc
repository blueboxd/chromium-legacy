// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"

#include <string>

#include "base/stl_util.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace app_time {

namespace {
base::Time TimeFromString(const std::string& str) {
  base::Time timestamp;
  EXPECT_TRUE(base::Time::FromString(str.c_str(), &timestamp));
  return timestamp;
}

}  // namespace

using ActiveTimeTest = testing::Test;
using AppActivityTest = testing::Test;

TEST_F(ActiveTimeTest, CreateActiveTime) {
  const base::Time start = TimeFromString("11 Jan 2020 10:00:00 PST");
  const base::Time end = TimeFromString("11 Jan 2020 10:10:00 PST");

  // Create ActiveTime with the correct timestamps.
  const AppActivity::ActiveTime active_time(start, end);
  EXPECT_EQ(start, active_time.active_from());
  EXPECT_EQ(end, active_time.active_to());

  // Try to create ActiveTime with invalid ranges.
  EXPECT_DCHECK_DEATH(AppActivity::ActiveTime(start, start));
  EXPECT_DCHECK_DEATH(AppActivity::ActiveTime(end, start));
}

TEST_F(ActiveTimeTest, UpdateActiveTime) {
  AppActivity::ActiveTime active_time(
      TimeFromString("11 Jan 2020 10:00:00 PST"),
      TimeFromString("11 Jan 2020 10:10:00 PST"));
  const base::Time& start_equal_end = active_time.active_to();
  EXPECT_DCHECK_DEATH(active_time.set_active_from(start_equal_end));

  const base::Time start_after_end =
      active_time.active_to() + base::TimeDelta::FromSeconds(1);
  EXPECT_DCHECK_DEATH(active_time.set_active_from(start_after_end));

  const base::Time& end_equal_start = active_time.active_from();
  EXPECT_DCHECK_DEATH(active_time.set_active_to(end_equal_start));

  const base::Time end_before_start =
      active_time.active_from() - base::TimeDelta::FromSeconds(1);
  EXPECT_DCHECK_DEATH(active_time.set_active_to(end_before_start));
}

TEST_F(ActiveTimeTest, ActiveTimeTimestampComparisions) {
  const AppActivity::ActiveTime active_time(
      TimeFromString("11 Jan 2020 10:00:00 PST"),
      TimeFromString("11 Jan 2020 10:10:00 PST"));

  const base::Time contained = TimeFromString("11 Jan 2020 10:05:00 PST");
  EXPECT_TRUE(active_time.Contains(contained));
  EXPECT_FALSE(active_time.IsEarlierThan(contained));
  EXPECT_FALSE(active_time.IsLaterThan(contained));

  const base::Time before = TimeFromString("11 Jan 2020 09:58:00 PST");
  EXPECT_FALSE(active_time.Contains(before));
  EXPECT_FALSE(active_time.IsEarlierThan(before));
  EXPECT_TRUE(active_time.IsLaterThan(before));

  const base::Time after = TimeFromString("11 Jan 2020 10:11:00 PST");
  EXPECT_FALSE(active_time.Contains(after));
  EXPECT_TRUE(active_time.IsEarlierThan(after));
  EXPECT_FALSE(active_time.IsLaterThan(after));

  const base::Time& equal_start = active_time.active_from();
  EXPECT_FALSE(active_time.Contains(equal_start));
  EXPECT_FALSE(active_time.IsEarlierThan(equal_start));
  EXPECT_TRUE(active_time.IsLaterThan(equal_start));

  const base::Time& equal_end = active_time.active_to();
  EXPECT_FALSE(active_time.Contains(equal_end));
  EXPECT_TRUE(active_time.IsEarlierThan(equal_end));
  EXPECT_FALSE(active_time.IsLaterThan(equal_end));
}

TEST_F(AppActivityTest, RemoveActiveTimes) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  AppActivity activity(AppState::kAvailable);

  // Time interval that will be removed.
  base::Time start = base::Time::Now();
  activity.SetAppActive(start);
  task_environment.FastForwardBy(base::TimeDelta::FromMinutes(10));
  base::Time end = base::Time::Now();
  activity.SetAppInactive(end);
  const AppActivity::ActiveTime to_remove(start, end);

  // Time interval that will be trimmed.
  start = base::Time::Now();
  activity.SetAppActive(start);
  task_environment.FastForwardBy(base::TimeDelta::FromMinutes(5));
  const base::Time report_time = base::Time::Now();
  task_environment.FastForwardBy(base::TimeDelta::FromMinutes(5));
  end = base::Time::Now();
  activity.SetAppInactive(end);
  const AppActivity::ActiveTime to_trim(start, end);

  // Time interval that will be kept.
  start = base::Time::Now();
  activity.SetAppActive(start);
  task_environment.FastForwardBy(base::TimeDelta::FromMinutes(10));
  end = base::Time::Now();
  activity.SetAppInactive(end);
  const AppActivity::ActiveTime to_keep(start, end);

  EXPECT_EQ(3u, activity.active_times().size());
  EXPECT_TRUE(base::Contains(activity.active_times(), to_remove));
  EXPECT_TRUE(base::Contains(activity.active_times(), to_trim));
  EXPECT_TRUE(base::Contains(activity.active_times(), to_keep));

  activity.RemoveActiveTimeEarlierThan(report_time);

  EXPECT_EQ(2u, activity.active_times().size());
  EXPECT_FALSE(base::Contains(activity.active_times(), to_remove));
  EXPECT_TRUE(base::Contains(activity.active_times(), to_keep));

  const AppActivity::ActiveTime trimmed(report_time, to_trim.active_to());
  EXPECT_TRUE(base::Contains(activity.active_times(), trimmed));
}

// TODO(agawronska) : Add more tests for app activity.

}  // namespace app_time
}  // namespace chromeos
