// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/background_task_coordinator.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/task_environment.h"
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

struct TestData {
  // Impression data as the input.
  std::vector<test::ImpressionTestData> impression_test_data;

  // Notification entries as the input.
  std::vector<NotificationEntry> notification_entries;
};

class BackgroundTaskCoordinatorTest : public testing::Test {
 public:
  BackgroundTaskCoordinatorTest() = default;
  ~BackgroundTaskCoordinatorTest() override = default;

 protected:
  void SetUp() override {
    // Setup configuration used by this test.
    config_.max_daily_shown_all_type = 3;
    config_.max_daily_shown_per_type = 2;
    config_.suppression_duration = base::TimeDelta::FromDays(3);

    auto background_task =
        std::make_unique<test::MockNotificationBackgroundTaskScheduler>();
    background_task_ = background_task.get();
    coordinator_ = BackgroundTaskCoordinator::Create(std::move(background_task),
                                                     &config_, &clock_);
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
                                         std::move(client_states));
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
    TestData test_data{kSingleClientImpressionTestData, {entry}};
    ScheduleTask(test_data);
  }

 private:
  base::test::TaskEnvironment task_environment_;
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

}  // namespace
}  // namespace notifications
