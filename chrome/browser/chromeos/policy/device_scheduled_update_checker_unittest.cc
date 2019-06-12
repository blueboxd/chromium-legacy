// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_scheduled_update_checker.h"

#include <sstream>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

void DecodeJsonStringAndNormalize(const std::string& json_string,
                                  base::Value* value) {
  base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
  base::Optional<base::Value> read_value = reader.ReadToValue(json_string);
  ASSERT_EQ(reader.GetErrorMessage(), "");
  ASSERT_TRUE(read_value.has_value());
  *value = std::move(read_value.value());
}

// Creates a JSON policy for daily device scheduled update checks.
std::string CreateDailyScheduledUpdateCheckPolicyJson(int hour, int minute) {
  return base::StringPrintf(
      "{\"update_check_time\": {\"hour\": %d, \"minute\":  %d}, \"frequency\": "
      "\"DAILY\"}",
      hour, minute);
}

// Creates a JSON policy for weekly device scheduled update checks.
std::string CreateWeeklyScheduledUpdateCheckPolicyJson(
    int hour,
    int minute,
    const std::string& day_of_week) {
  return base::StringPrintf(
      "{\"update_check_time\": {\"hour\": %d, \"minute\":  %d}, \"frequency\": "
      "\"WEEKLY\", \"day_of_week\": \"%s\"}",
      hour, minute, day_of_week.c_str());
}

// Creates a JSON policy for monthly device scheduled update checks.
std::string CreateMonthlyScheduledUpdateCheckPolicyJson(int hour,
                                                        int minute,
                                                        int day_of_month) {
  return base::StringPrintf(
      "{\"update_check_time\": {\"hour\": %d, \"minute\":  %d}, \"frequency\": "
      "\"MONTHLY\", \"day_of_month\": %d}",
      hour, minute, day_of_month);
}

// Convert the string day of week to an integer value suitable for
// |base::Time::Exploded::day_of_week|. Returns false if |day_of_week| is
// invalid.
std::string ExplodedTimeDayOfWeekToStringDayOfWeek(int day_of_week) {
  if (day_of_week == 0)
    return "SUNDAY";
  if (day_of_week == 1)
    return "MONDAY";
  if (day_of_week == 2)
    return "TUESDAY";
  if (day_of_week == 3)
    return "WEDNESDAY";
  if (day_of_week == 4)
    return "THURSDAY";
  if (day_of_week == 5)
    return "FRIDAY";
  return "SATURDAY";
}

// Increment |input_base_time| by |hours| and put the result in
// |output_base_time| and |output_exploded_time|.
void IncrementTimeByHours(const base::Time& input_time,
                          int hours,
                          base::Time* output_time,
                          base::Time::Exploded* output_exploded_time) {
  *output_time = input_time + base::TimeDelta::FromHours(hours);
  output_time->LocalExplode(output_exploded_time);
}

}  // namespace

class DeviceScheduledUpdateCheckerForTest
    : public DeviceScheduledUpdateChecker {
 public:
  DeviceScheduledUpdateCheckerForTest(chromeos::CrosSettings* cros_settings,
                                      const base::Clock* clock,
                                      const base::TickClock* tick_clock)
      : DeviceScheduledUpdateChecker(cros_settings),
        clock_(clock),
        tick_clock_(tick_clock) {}
  ~DeviceScheduledUpdateCheckerForTest() override = default;

  int GetUpdateChecks() const { return update_checks_; }

  void SimulateCalculateNextUpdateCheckFailure(bool simulate) {
    simulate_calculate_next_update_check_failure_ = simulate;
  }

 private:
  void UpdateCheck() override {
    ++update_checks_;
    DeviceScheduledUpdateChecker::UpdateCheck();
  }

  base::Optional<base::Time> CalculateNextUpdateCheckTime(
      base::Time cur_local_time) override {
    if (simulate_calculate_next_update_check_failure_)
      return base::nullopt;
    return DeviceScheduledUpdateChecker::CalculateNextUpdateCheckTime(
        cur_local_time);
  }

  base::Time GetCurrentTime() override { return clock_->Now(); }

  base::TimeTicks GetTicksSinceBoot() override {
    return tick_clock_->NowTicks();
  }

  // Clock to use to get current time.
  const base::Clock* const clock_;

  // Clock to use to calculate time ticks.
  const base::TickClock* const tick_clock_;

  // Number of times |UpdateCheck| is called.
  int update_checks_ = 0;

  // If set, then |CalculateNextUpdateCheckTime| returns base::nullopt.
  bool simulate_calculate_next_update_check_failure_ = false;

  DISALLOW_COPY_AND_ASSIGN(DeviceScheduledUpdateCheckerForTest);
};

class DeviceScheduledUpdateCheckerTest : public testing::Test {
 protected:
  DeviceScheduledUpdateCheckerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO_MOCK_TIME) {
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(
        scoped_task_environment_.GetMockTickClock());
    device_scheduled_update_checker_ =
        std::make_unique<DeviceScheduledUpdateCheckerForTest>(
            chromeos::CrosSettings::Get(),
            scoped_task_environment_.GetMockClock(),
            scoped_task_environment_.GetMockTickClock());
  }

  ~DeviceScheduledUpdateCheckerTest() override {
    device_scheduled_update_checker_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  // Sets a daily update check policy and returns true iff it's scheduled
  // correctly. |hours_from_now| must be > 0.
  bool CheckDailyUpdateCheck(int hours_fom_now) {
    DCHECK_GT(hours_fom_now, 0);
    // Calculate time from one hour from now and set the update check policy to
    // happen daily at that time.
    base::Time update_check_time;
    base::Time::Exploded update_check_exploded_time;
    const int increment_update_check_hours_by = hours_fom_now;
    base::TimeDelta delay_from_now =
        base::TimeDelta::FromHours(increment_update_check_hours_by);
    IncrementTimeByHours(scoped_task_environment_.GetMockClock()->Now(),
                         increment_update_check_hours_by, &update_check_time,
                         &update_check_exploded_time);

    base::Value scheduled_update_check_value;
    DecodeJsonStringAndNormalize(
        CreateDailyScheduledUpdateCheckPolicyJson(
            update_check_exploded_time.hour, update_check_exploded_time.minute),
        &scheduled_update_check_value);

    // Set a new scheduled update setting, fast forward to right before the
    // expected update and then check if an update check is not scheduled.
    const base::TimeDelta small_delay = base::TimeDelta::FromMilliseconds(1);
    cros_settings_.device_settings()->Set(chromeos::kDeviceScheduledUpdateCheck,
                                          scheduled_update_check_value);
    int update_checks = device_scheduled_update_checker_->GetUpdateChecks();
    scoped_task_environment_.FastForwardBy(delay_from_now - small_delay);
    if (device_scheduled_update_checker_->GetUpdateChecks() != update_checks)
      return false;

    // Fast forward to the expected update check time and then check if the
    // update check is scheduled.
    update_checks = device_scheduled_update_checker_->GetUpdateChecks();
    scoped_task_environment_.FastForwardBy(small_delay);
    if (device_scheduled_update_checker_->GetUpdateChecks() !=
        (update_checks + 1)) {
      return false;
    }

    // An update check should happen every day since the policy is set to daily.
    update_checks = device_scheduled_update_checker_->GetUpdateChecks();
    const int days = 5;
    scoped_task_environment_.FastForwardBy(base::TimeDelta::FromDays(days));
    if (device_scheduled_update_checker_->GetUpdateChecks() !=
        (update_checks + days)) {
      return false;
    }

    return true;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<DeviceScheduledUpdateCheckerForTest>
      device_scheduled_update_checker_;
  chromeos::ScopedTestingCrosSettings cros_settings_;

  DISALLOW_COPY_AND_ASSIGN(DeviceScheduledUpdateCheckerTest);
};

TEST_F(DeviceScheduledUpdateCheckerTest, CheckIfDailyUpdateCheckIsScheduled) {
  EXPECT_TRUE(CheckDailyUpdateCheck(1 /* hours_from_now */));
}

TEST_F(DeviceScheduledUpdateCheckerTest, CheckIfWeeklyUpdateCheckIsScheduled) {
  // Set the first update check to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then weekly after.
  base::Time update_check_time;
  base::Time::Exploded update_check_exploded_time;
  const int increment_update_check_hours_by = 49;
  base::TimeDelta delay_from_now =
      base::TimeDelta::FromHours(increment_update_check_hours_by);
  IncrementTimeByHours(scoped_task_environment_.GetMockClock()->Now(),
                       increment_update_check_hours_by, &update_check_time,
                       &update_check_exploded_time);

  base::Value scheduled_update_check_value;
  ASSERT_NO_FATAL_FAILURE(DecodeJsonStringAndNormalize(
      CreateWeeklyScheduledUpdateCheckPolicyJson(
          update_check_exploded_time.hour, update_check_exploded_time.minute,
          ExplodedTimeDayOfWeekToStringDayOfWeek(
              update_check_exploded_time.day_of_week)),
      &scheduled_update_check_value));

  // Set a new scheduled update setting, fast forward to right before the
  // expected update and then check if an update check is not scheduled.
  const base::TimeDelta small_delay = base::TimeDelta::FromMilliseconds(1);
  cros_settings_.device_settings()->Set(chromeos::kDeviceScheduledUpdateCheck,
                                        scheduled_update_check_value);
  scoped_task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateChecks(), 0);

  // Fast forward to the expected update check time and then check if the update
  // check is scheduled.
  scoped_task_environment_.FastForwardBy(small_delay);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateChecks(), 1);

  // An update check should happen weekly since the policy is set to weekly.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromDays(7));
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateChecks(), 2);
}

TEST_F(DeviceScheduledUpdateCheckerTest, CheckIfMonthlyUpdateCheckIsScheduled) {
  // Set the first update check to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then monthly after.
  base::Time update_check_time;
  base::Time::Exploded update_check_exploded_time;
  const int increment_update_check_hours_by = 49;
  base::TimeDelta delay_from_now =
      base::TimeDelta::FromHours(increment_update_check_hours_by);
  IncrementTimeByHours(scoped_task_environment_.GetMockClock()->Now(),
                       increment_update_check_hours_by, &update_check_time,
                       &update_check_exploded_time);

  base::Value scheduled_update_check_value;
  ASSERT_NO_FATAL_FAILURE(DecodeJsonStringAndNormalize(
      CreateMonthlyScheduledUpdateCheckPolicyJson(
          update_check_exploded_time.hour, update_check_exploded_time.minute,
          update_check_exploded_time.day_of_month),
      &scheduled_update_check_value));

  // Set a new scheduled update setting, fast forward to right before the
  // expected update and then check if an update check is not scheduled.
  const base::TimeDelta small_delay = base::TimeDelta::FromMilliseconds(1);
  cros_settings_.device_settings()->Set(chromeos::kDeviceScheduledUpdateCheck,
                                        scheduled_update_check_value);
  scoped_task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateChecks(), 0);

  // Fast forward to the expected update check time and then check if the update
  // check is scheduled.
  scoped_task_environment_.FastForwardBy(small_delay);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateChecks(), 1);

  // The next update check should happen at the same day of month next month.
  base::Optional<base::Time> next_check_time =
      update_checker_internal::IncrementMonthAndSetDayOfMonth(
          update_check_exploded_time, update_check_exploded_time.day_of_month);
  // This should be always set in a virtual time environment.
  EXPECT_TRUE(next_check_time);
  delay_from_now =
      next_check_time.value() - scoped_task_environment_.GetMockClock()->Now();
  base::Time::Exploded next_check_exploded_time;
  next_check_time->LocalExplode(&next_check_exploded_time);
  scoped_task_environment_.FastForwardBy(delay_from_now);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateChecks(), 2);
}

// Checks if an update check timer can't be started, retries are scheduled to
// recover from transient errors.
TEST_F(DeviceScheduledUpdateCheckerTest, CheckRetryLogicEventualSuccess) {
  // This will simulate an error while calculating the next update check time
  // and will result in no update checks happening till its set.
  device_scheduled_update_checker_->SimulateCalculateNextUpdateCheckFailure(
      true);

  // Calculate time from one hour from now and set the update check policy to
  // happen daily at that time.
  base::Time update_check_time;
  base::Time::Exploded update_check_exploded_time;
  const int increment_update_check_hours_by = 1;
  IncrementTimeByHours(scoped_task_environment_.GetMockClock()->Now(),
                       increment_update_check_hours_by, &update_check_time,
                       &update_check_exploded_time);

  base::Value scheduled_update_check_value;
  ASSERT_NO_FATAL_FAILURE(DecodeJsonStringAndNormalize(
      CreateDailyScheduledUpdateCheckPolicyJson(
          update_check_exploded_time.hour, update_check_exploded_time.minute),
      &scheduled_update_check_value));

  // Fast forward time by less than (max retries * retry period) and check that
  // no update has occurred due to failure being simulated.
  cros_settings_.device_settings()->Set(chromeos::kDeviceScheduledUpdateCheck,
                                        scheduled_update_check_value);
  scoped_task_environment_.FastForwardBy(
      (update_checker_internal::kMaxRetryUpdateCheckIterations - 2) *
      update_checker_internal::kStartUpdateCheckTimerRetryTime);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateChecks(), 0);

  // Reset failure mode and fast forward by the retry period. This time it
  // should succeed in setting an update check timer. No update checks should
  // happen yet but a check has just been scheduled.
  device_scheduled_update_checker_->SimulateCalculateNextUpdateCheckFailure(
      false);
  scoped_task_environment_.FastForwardBy(
      update_checker_internal::kStartUpdateCheckTimerRetryTime);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateChecks(), 0);

  // Check if update checks happen daily from now on.
  const int days = 2;
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromDays(days));
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateChecks(), days);
}

// Checks if an update check timer can't be started due to a calculation
// failure, retries are capped.
TEST_F(DeviceScheduledUpdateCheckerTest,
       CheckRetryLogicCapWithCalculationFailure) {
  // This will simulate an error while calculating the next update check time
  // and will result in no update checks happening till its set.
  device_scheduled_update_checker_->SimulateCalculateNextUpdateCheckFailure(
      true);
  EXPECT_FALSE(CheckDailyUpdateCheck(1 /* hours_from_now */));

  // Fast forward by max retries * retry period and check that no update has
  // happened since failure mode is still set.
  scoped_task_environment_.FastForwardBy(
      update_checker_internal::kMaxRetryUpdateCheckIterations *
      update_checker_internal::kStartUpdateCheckTimerRetryTime);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateChecks(), 0);

  // At this point all state has been reset. Reset failure mode and check if
  // daily update checks happen.
  device_scheduled_update_checker_->SimulateCalculateNextUpdateCheckFailure(
      false);
  EXPECT_TRUE(CheckDailyUpdateCheck(1 /* hours_from_now */));
}

// Checks if an update check timer can't be started due to a timer start
// failure, retries are capped.
TEST_F(DeviceScheduledUpdateCheckerTest,
       CheckRetryLogicCapWithTimerStartFailure) {
  // This will simulate an error while starting the update check timer.
  // and will result in no update checks happening till its set.
  chromeos::FakePowerManagerClient::Get()->simulate_start_arc_timer_failure(
      true);
  EXPECT_FALSE(CheckDailyUpdateCheck(1 /* hours_from_now */));

  // Fast forward by max retries * retry period and check that no update has
  // happened since failure mode is still set.
  scoped_task_environment_.FastForwardBy(
      update_checker_internal::kMaxRetryUpdateCheckIterations *
      update_checker_internal::kStartUpdateCheckTimerRetryTime);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateChecks(), 0);

  // At this point all state has been reset. Reset failure mode and check if
  // daily update checks happen.
  chromeos::FakePowerManagerClient::Get()->simulate_start_arc_timer_failure(
      false);
  EXPECT_TRUE(CheckDailyUpdateCheck(1 /* hours_from_now */));
}

}  // namespace policy
