// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/background_task_coordinator.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/test/fake_clock.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace notifications {
namespace {

using Notifications = BackgroundTaskCoordinator::Notifications;
using ClientStates = BackgroundTaskCoordinator::ClientStates;

const char kGuid[] = "1234";
const std::vector<test::ImpressionTestData> kSingleClientImpressionTestData = {
    {SchedulerClientType::kTest1,
     1 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */}};

const std::vector<test::ImpressionTestData> kClientsImpressionTestData = {
    {SchedulerClientType::kTest1,
     1 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */},
    {SchedulerClientType::kTest2,
     2 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */}};

base::TimeDelta NoopTimeRandomizer(const base::TimeDelta& time_window) {
  return base::TimeDelta();
}

struct TestData {
  // Impression data as the input.
  std::vector<test::ImpressionTestData> impression_test_data;

  // Notification entries as the input.
  std::vector<NotificationEntry> notification_entries;

  // The type of current background task.
  SchedulerTaskTime task_start_time = SchedulerTaskTime::kMorning;
};

class BackgroundTaskCoordinatorTest : public testing::Test {
 public:
  BackgroundTaskCoordinatorTest() = default;
  ~BackgroundTaskCoordinatorTest() override = default;

 protected:
  void SetUp() override {
    // Setup configuration used by this test.
    config_.morning_task_hour = 6;
    config_.evening_task_hour = 18;
    config_.max_daily_shown_all_type = 3;
    config_.max_daily_shown_per_type = 2;
    config_.suppression_duration = base::TimeDelta::FromDays(3);

    auto background_task =
        std::make_unique<test::MockNotificationBackgroundTaskScheduler>();
    background_task_ = background_task.get();
    coordinator_ = std::make_unique<BackgroundTaskCoordinator>(
        std::move(background_task), &config_,
        base::BindRepeating(&NoopTimeRandomizer, base::TimeDelta()), &clock_);
  }

  test::MockNotificationBackgroundTaskScheduler* background_task() {
    return background_task_;
  }

  SchedulerConfig* config() { return &config_; }

  void SetNow(const char* now_str) { clock_.SetNow(now_str); }

  base::Time GetTime(const char* time_str) {
    return test::FakeClock::GetTime(time_str);
  }

  void ScheduleTask(const TestData& test_data) {
    test_data_ = test_data;
    test::AddImpressionTestData(test_data_.impression_test_data,
                                &client_states_);
    std::map<SchedulerClientType, const ClientState*> client_states;
    for (const auto& type : client_states_) {
      client_states.emplace(type.first, type.second.get());
    }

    Notifications notifications;
    for (const auto& entry : test_data_.notification_entries) {
      notifications[entry.type].emplace_back(&entry);
    }
    coordinator_->ScheduleBackgroundTask(std::move(notifications),
                                         std::move(client_states),
                                         test_data_.task_start_time);
  }

  void TestScheduleNewNotification(const char* now,
                                   const char* expected_task_start_time) {
    SetNow(now);
    EXPECT_CALL(*background_task(), Cancel()).Times(0);
    auto expected_window_start =
        GetTime(expected_task_start_time) - GetTime(now);
    EXPECT_CALL(*background_task(),
                Schedule(_, expected_window_start,
                         expected_window_start +
                             config()->background_task_window_duration));

    NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
    TestData test_data{
        kSingleClientImpressionTestData, {entry}, SchedulerTaskTime::kUnknown};
    ScheduleTask(test_data);
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  test::FakeClock clock_;
  SchedulerConfig config_;
  std::unique_ptr<BackgroundTaskCoordinator> coordinator_;
  test::MockNotificationBackgroundTaskScheduler* background_task_;
  TestData test_data_;
  std::map<SchedulerClientType, std::unique_ptr<ClientState>> client_states_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTaskCoordinatorTest);
};

// No notification persisted, then no background task needs to be scheduled.
// And current task should be canceled.
TEST_F(BackgroundTaskCoordinatorTest, NoNotification) {
  EXPECT_CALL(*background_task(), Cancel());
  EXPECT_CALL(*background_task(), Schedule(_, _, _)).Times(0);
  TestData test_data;
  test_data.impression_test_data = kSingleClientImpressionTestData;
  ScheduleTask(test_data);
}

// In a morning task, find one notification and schedule an evening task.
TEST_F(BackgroundTaskCoordinatorTest, InMorningScheduleEvening) {
  const char kNow[] = "04/25/20 01:00:00 AM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task this evening.
  auto expected_window_start = GetTime("04/25/20 18:00:00 PM") - GetTime(kNow);
  EXPECT_CALL(*background_task(),
              Schedule(_, expected_window_start,
                       expected_window_start +
                           config()->background_task_window_duration));

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{
      kSingleClientImpressionTestData, {entry}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

// In morning task, schedule evening task but throttled, schedule to next
// morning.
TEST_F(BackgroundTaskCoordinatorTest, InMorningScheduleEveningThrottled) {
  const char kNow[] = "04/25/20 02:00:00 PM";
  SetNow(kNow);

  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task next morning.
  EXPECT_CALL(*background_task(),
              Schedule(_, GetTime("04/26/20 06:00:00 AM") - GetTime(kNow), _));

  auto impression_data = kSingleClientImpressionTestData;
  Impression impression;
  impression.create_time = GetTime("04/25/20 01:00:00 AM");
  impression_data.back().impressions.emplace_back(impression);

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{impression_data, {entry}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

// In an evening task, schedule background task to run next morning.
TEST_F(BackgroundTaskCoordinatorTest, InEveningScheduleNextMorning) {
  const char kNow[] = "04/25/20 18:00:00 PM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task next morning.
  auto expected_window_start = GetTime("04/26/20 06:00:00 AM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{
      kSingleClientImpressionTestData, {entry}, SchedulerTaskTime::kEvening};
  ScheduleTask(test_data);
}

// In an evening task, schedule background task to run next morning, even if we
// reached the daily max.
TEST_F(BackgroundTaskCoordinatorTest, InEveningScheduleNextMorningThrottled) {
  const char kNow[] = "04/25/20 18:00:00 PM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task next morning.
  auto expected_window_start = GetTime("04/26/20 06:00:00 AM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  // We have reached daily max.
  auto impression_data = kSingleClientImpressionTestData;
  Impression impression;
  impression.create_time = GetTime("04/25/20 01:00:00 AM");
  impression_data.back().impressions.emplace_back(impression);

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{
      kSingleClientImpressionTestData, {entry}, SchedulerTaskTime::kEvening};
  ScheduleTask(test_data);
}

// Suppression will result in background task scheduled after suppression
// expired.
TEST_F(BackgroundTaskCoordinatorTest, Suppression) {
  const char kNow[] = "04/25/20 06:00:00 AM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task in the morning after suppression expired.
  auto expected_window_start = GetTime("04/28/20 06:00:00 AM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  auto impression_data = kSingleClientImpressionTestData;
  impression_data.back().suppression_info = SuppressionInfo(
      GetTime("04/25/20 00:00:00 AM"), base::TimeDelta::FromDays(3));

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{impression_data, {entry}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

// If two different types want to schedule at different times, pick the earilier
// one.
TEST_F(BackgroundTaskCoordinatorTest, ScheduleEarlierTime) {
  const char kNow[] = "04/25/20 01:00:00 AM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // kTest1 type will run this evening, kTest2 will run task 3 days later.
  // Expected to run the earilier task.
  auto expected_window_start = GetTime("04/25/20 18:00:00 PM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  NotificationEntry entry1(SchedulerClientType::kTest1, kGuid);
  NotificationEntry entry2(SchedulerClientType::kTest2, "guid_entry2");
  auto impression_data = kClientsImpressionTestData;
  impression_data[0].suppression_info = SuppressionInfo(
      GetTime("04/25/20 00:00:00 AM"), base::TimeDelta::FromDays(3));
  TestData test_data{
      impression_data, {entry1, entry2}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

// If reached |max_daily_shown_all_type|, background task should run tomorrow.
TEST_F(BackgroundTaskCoordinatorTest, InMorningThrottledAllTypes) {
  const char kNow[] = "04/25/20 05:00:00 AM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run task next morning.
  auto expected_window_start = GetTime("04/26/20 06:00:00 AM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  auto impression_data = kClientsImpressionTestData;
  Impression impression;
  impression.create_time = GetTime("04/25/20 01:00:00 AM");

  // Make sure we reach daily max for all types.
  for (int i = 0; i < config()->max_daily_shown_all_type; i++)
    impression_data.back().impressions.emplace_back(impression);

  NotificationEntry entry(SchedulerClientType::kTest1, kGuid);
  TestData test_data{impression_data, {entry}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

// If reached |max_daily_shown_all_type| and all types have suppression,
// background task should run after one suppression expired.
TEST_F(BackgroundTaskCoordinatorTest, ThrottledAllTypesAndSuppression) {
  const char kNow[] = "04/25/20 05:00:00 AM";
  SetNow(kNow);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  // Expected to run after 3 days suppression ends.
  auto expected_window_start = GetTime("04/28/20 06:00:00 AM") - GetTime(kNow);
  EXPECT_CALL(*background_task(), Schedule(_, expected_window_start, _));

  auto impression_data = kClientsImpressionTestData;
  Impression impression;
  impression.create_time = GetTime("04/25/20 01:00:00 AM");

  // Make sure we reach daily max for all types.
  for (int i = 0; i < config()->max_daily_shown_all_type; i++)
    impression_data[1].impressions.emplace_back(impression);

  // Suppression for both types.
  impression_data[0].suppression_info = SuppressionInfo(
      GetTime("04/25/20 00:00:00 AM"), base::TimeDelta::FromDays(3));
  impression_data[1].suppression_info = SuppressionInfo(
      GetTime("04/25/20 00:00:00 AM"), base::TimeDelta::FromDays(4));

  NotificationEntry entry1(SchedulerClientType::kTest1, "test_guid_1");
  NotificationEntry entry2(SchedulerClientType::kTest2, "test_guid_2");
  TestData test_data{
      impression_data, {entry1, entry2}, SchedulerTaskTime::kMorning};
  ScheduleTask(test_data);
}

// Schedules a new notification when Chrome is not running in a background task
// at different time of a day.
TEST_F(BackgroundTaskCoordinatorTest, ScheduleNewNotification) {
  TestScheduleNewNotification("04/25/20 01:00:00 AM", "04/25/20 06:00:00 AM");
  TestScheduleNewNotification("04/25/20 07:00:00 AM", "04/25/20 18:00:00 PM");
  TestScheduleNewNotification("04/25/20 18:30:00 PM", "04/26/20 06:00:00 AM");
}

// Test to verify the default time randomizer.
TEST_F(BackgroundTaskCoordinatorTest, DefaultTimeRandomizer) {
  EXPECT_EQ(BackgroundTaskCoordinator::DefaultTimeRandomizer(base::TimeDelta()),
            base::TimeDelta());
  auto time_window = base::TimeDelta::FromHours(1);
  auto delta = BackgroundTaskCoordinator::DefaultTimeRandomizer(time_window);
  EXPECT_LT(delta, time_window);
  EXPECT_GE(delta, base::TimeDelta());
}

}  // namespace
}  // namespace notifications
