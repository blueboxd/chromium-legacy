// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"

#include <algorithm>

#include "base/guid.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/test/fake_clock.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

const char kFakeNow[] = "01/01/18 01:23:45 AM";

class SchedulerUtilsTest : public testing::Test {
 public:
  SchedulerUtilsTest() {}
  ~SchedulerUtilsTest() override = default;

  void SetUp() override { config_.initial_daily_shown_per_type = 100; }

  void InitFakeDay() {
    clock_.SetNow(kFakeNow);
    ToLocalHour(0, clock_.Now(), 0, &beginning_of_today_);
  }

  void CreateFakeImpressions(std::vector<base::Time> times,
                             std::deque<Impression>& impressions) {
    impressions.clear();
    for (auto time : times) {
      impressions.emplace_back(SchedulerClientType::kTest1,
                               base::GenerateGUID(), time);
    }
    std::sort(impressions.begin(), impressions.end(),
              [](const Impression& lhs, const Impression& rhs) {
                return lhs.create_time < rhs.create_time;
              });
  }

 protected:
  SchedulerConfig& config() { return config_; }
  test::FakeClock* clock() { return &clock_; }
  base::Time& beginning_of_today() { return beginning_of_today_; }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  test::FakeClock clock_;
  SchedulerConfig config_;
  base::Time beginning_of_today_;
  DISALLOW_COPY_AND_ASSIGN(SchedulerUtilsTest);
};

// Verifies we can get the correct time stamp at certain hour in yesterday.
TEST_F(SchedulerUtilsTest, ToLocalHour) {
  base::Time today, another_day, expected;

  // Timestamp of another day in the past.
  EXPECT_TRUE(base::Time::FromString("10/15/07 12:45:12 PM", &today));
  EXPECT_TRUE(ToLocalHour(6, today, -1, &another_day));
  EXPECT_TRUE(base::Time::FromString("10/14/07 06:00:00 AM", &expected));
  EXPECT_EQ(expected, another_day);

  EXPECT_TRUE(base::Time::FromString("03/25/19 00:00:00 AM", &today));
  EXPECT_TRUE(ToLocalHour(0, today, -1, &another_day));
  EXPECT_TRUE(base::Time::FromString("03/24/19 00:00:00 AM", &expected));
  EXPECT_EQ(expected, another_day);

  // Timestamp of the same day.
  EXPECT_TRUE(base::Time::FromString("03/25/19 00:00:00 AM", &today));
  EXPECT_TRUE(ToLocalHour(0, today, 0, &another_day));
  EXPECT_TRUE(base::Time::FromString("03/25/19 00:00:00 AM", &expected));
  EXPECT_EQ(expected, another_day);

  // Timestamp of another day in the future.
  EXPECT_TRUE(base::Time::FromString("03/25/19 06:35:27 AM", &today));
  EXPECT_TRUE(ToLocalHour(16, today, 7, &another_day));
  EXPECT_TRUE(base::Time::FromString("04/01/19 16:00:00 PM", &expected));
  EXPECT_EQ(expected, another_day);
}

TEST_F(SchedulerUtilsTest, NotificationsShownToday) {
  // Create fake client.
  auto new_client = CreateNewClientState(SchedulerClientType::kTest1, config());
  InitFakeDay();
  base::Time now = clock()->Now();
  // Test case 1:
  int count = NotificationsShownToday(new_client.get(), clock());
  EXPECT_EQ(count, 0);

  // Test case 2:
  std::vector<base::Time> create_times = {
      now /*today*/,
      beginning_of_today() + base::TimeDelta::FromDays(1) /*tomorrow*/,
      beginning_of_today() - base::TimeDelta::FromSeconds(1) /*yesterday*/,
      beginning_of_today() + base::TimeDelta::FromSeconds(1) /*today*/,
      beginning_of_today() /*today*/};

  CreateFakeImpressions(create_times, new_client->impressions);
  count = NotificationsShownToday(new_client.get(), clock());
  EXPECT_EQ(count, 3);

  // Test case 3:
  create_times = {
      beginning_of_today() - base::TimeDelta::FromSeconds(2), /*yesterday*/
      beginning_of_today() - base::TimeDelta::FromDays(2),    /*tomorrow*/
  };
  CreateFakeImpressions(create_times, new_client->impressions);
  count = NotificationsShownToday(new_client.get(), clock());
  EXPECT_EQ(count, 0);

  // Test case 4:
  create_times = {
      now /*today*/, now - base::TimeDelta::FromSeconds(1) /*today*/,
      beginning_of_today() - base::TimeDelta::FromSeconds(1) /*yesterday*/,
      beginning_of_today() + base::TimeDelta::FromDays(1) /*tomorrow*/};
  CreateFakeImpressions(create_times, new_client->impressions);
  count = NotificationsShownToday(new_client.get(), clock());
  EXPECT_EQ(count, 2);
}
}  // namespace
}  // namespace notifications
