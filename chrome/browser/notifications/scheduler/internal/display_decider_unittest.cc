// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/display_decider.h"

#include <map>
#include <memory>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/distribution_policy.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

// Default suppression info used in this test.
const SuppressionInfo kSuppressionInfo =
    SuppressionInfo(base::Time::Now(), base::TimeDelta::FromDays(56));

// Initial state for test cases with a single registered client.
const std::vector<test::ImpressionTestData> kSingleClientImpressionTestData = {
    {SchedulerClientType::kTest1,
     2 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */}};

const std::vector<test::ImpressionTestData> kClientsImpressionTestData = {
    {SchedulerClientType::kTest1,
     2 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */},
    {SchedulerClientType::kTest2,
     5 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */},
    {SchedulerClientType::kTest3,
     0 /* current_max_daily_show */,
     {},
     kSuppressionInfo}};

struct TestData {
  // Impression data as the input.
  std::vector<test::ImpressionTestData> impression_test_data;

  // Notification entries as the input.
  std::vector<NotificationEntry> notification_entries;

  // The type of current background task.
  SchedulerTaskTime task_start_time = SchedulerTaskTime::kUnknown;

  // Expected output data.
  DisplayDecider::Results expected;
};

std::string DebugString(const DisplayDecider::Results& results) {
  std::string debug_string("notifications_to_show: \n");
  for (const auto& guid : results)
    debug_string += base::StringPrintf("%s ", guid.c_str());

  return debug_string;
}

// TODO(xingliu): Add more test cases.
class DisplayDeciderTest : public testing::Test {
 public:
  DisplayDeciderTest() = default;
  ~DisplayDeciderTest() override = default;

  void SetUp() override {
    // Setup configuration used by this test.
    config_.morning_task_hour = 7;
    config_.evening_task_hour = 18;
    config_.max_daily_shown_all_type = 3;
  }

 protected:
  // Initializes a test case with input data.
  void RunTestCase(const TestData& test_data) {
    test_data_ = test_data;
    test::AddImpressionTestData(test_data_.impression_test_data,
                                &client_states_);

    DisplayDecider::Notifications notifications;
    for (const auto& entry : test_data_.notification_entries) {
      notifications[entry.type].emplace_back(&entry);
    }
    std::vector<SchedulerClientType> clients;

    std::map<SchedulerClientType, const ClientState*> client_states;
    for (const auto& type : client_states_) {
      client_states.emplace(type.first, type.second.get());
      clients.emplace_back(type.first);
    }

    // Copy test inputs into |decider_|.
    decider_ =
        DisplayDecider::Create(&config_, clients, DistributionPolicy::Create());
    decider_->FindNotificationsToShow(
        test_data_.task_start_time, std::move(notifications),
        std::move(client_states), &results_);

    // Verify output.
    EXPECT_EQ(results_, test_data_.expected)
        << "Actual result: \n"
        << DebugString(results_) << " \n Expected result: \n"
        << DebugString(test_data_.expected);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  TestData test_data_;
  SchedulerConfig config_;

  std::map<SchedulerClientType, std::unique_ptr<ClientState>> client_states_;

  // Test target class and output.
  std::unique_ptr<DisplayDecider> decider_;
  DisplayDecider::Results results_;

  DISALLOW_COPY_AND_ASSIGN(DisplayDeciderTest);
};

TEST_F(DisplayDeciderTest, NoNotification) {
  TestData data{kClientsImpressionTestData,
                {},
                SchedulerTaskTime::kEvening,
                DisplayDecider::Results()};
  RunTestCase(data);
}

// Simple test case to verify new notifcaiton can be selected to show.
TEST_F(DisplayDeciderTest, PickNewMorning) {
  NotificationEntry entry(SchedulerClientType::kTest1, "guid123");
  DisplayDecider::Results expected = {"guid123"};
  TestData data{kSingleClientImpressionTestData,
                {entry},
                SchedulerTaskTime::kMorning,
                std::move(expected)};
  RunTestCase(data);
}

}  // namespace
}  // namespace notifications
