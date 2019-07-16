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
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/os_and_policies_update_checker.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/policy/core/common/policy_service.h"
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

// Increment |input_time| by |delta| and put the result in |output_base_time|
// and |output_exploded_time|.
void IncrementTimeByHours(base::Time input_time,
                          base::TimeDelta delta,
                          base::Time* output_time,
                          base::Time::Exploded* output_exploded_time) {
  *output_time = input_time + delta;
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

  ~DeviceScheduledUpdateCheckerForTest() override {
    TestingBrowserProcess::GetGlobal()->ShutdownBrowserPolicyConnector();
  }

  int GetUpdateCheckTimerExpirations() const {
    return update_check_timer_expirations_;
  }

  int GetUpdateCheckCompletions() const { return update_check_completions_; }

  void SimulateCalculateNextUpdateCheckFailure(bool simulate) {
    simulate_calculate_next_update_check_failure_ = simulate;
  }

 private:
  void OnUpdateCheckTimerExpired() override {
    ++update_check_timer_expirations_;
    DeviceScheduledUpdateChecker::OnUpdateCheckTimerExpired();
  }

  void OnUpdateCheckCompletion(bool result) override {
    if (result)
      ++update_check_completions_;
    DeviceScheduledUpdateChecker::OnUpdateCheckCompletion(result);
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

  // Number of times |OnUpdateCheckTimerExpired| is called.
  int update_check_timer_expirations_ = 0;

  // Number of times |OnUpdateCheckCompletion| is called with |result| = true.
  int update_check_completions_ = 0;

  // If set, then |CalculateNextUpdateCheckTime| returns base::nullopt.
  bool simulate_calculate_next_update_check_failure_ = false;

  DISALLOW_COPY_AND_ASSIGN(DeviceScheduledUpdateCheckerForTest);
};

class DeviceScheduledUpdateCheckerTest : public testing::Test {
 protected:
  DeviceScheduledUpdateCheckerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO,
            base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME) {
    auto fake_update_engine_client =
        std::make_unique<chromeos::FakeUpdateEngineClient>();
    fake_update_engine_client_ = fake_update_engine_client.get();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        std::move(fake_update_engine_client));

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

  // Notifies status update from |fake_update_engine_client_| and runs scheduled
  // tasks to ensure that the pending policy refresh completes.
  void NotifyUpdateCheckStatus(
      chromeos::UpdateEngineClient::UpdateStatusOperation
          update_status_operation) {
    chromeos::UpdateEngineClient::Status status = {};
    status.status = update_status_operation;
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
    scoped_task_environment_.RunUntilIdle();
  }

  // Returns true only iff all stats match in
  // |device_scheduled_update_checker_|.
  bool CheckStats(int expected_update_checks,
                  int expected_update_check_requests,
                  int expected_update_check_completions) {
    if (device_scheduled_update_checker_->GetUpdateCheckTimerExpirations() !=
        expected_update_checks) {
      LOG(ERROR)
          << "Current update check timer expirations: "
          << device_scheduled_update_checker_->GetUpdateCheckTimerExpirations()
          << " Expected update check timer expirations: "
          << expected_update_checks;
      return false;
    }

    if (fake_update_engine_client_->request_update_check_call_count() !=
        expected_update_check_requests) {
      LOG(ERROR)
          << "Current update check requests: "
          << fake_update_engine_client_->request_update_check_call_count()
          << " Expected update check requests: "
          << expected_update_check_requests;
      return false;
    }

    if (device_scheduled_update_checker_->GetUpdateCheckCompletions() !=
        expected_update_check_completions) {
      LOG(ERROR)
          << "Current update check completions: "
          << device_scheduled_update_checker_->GetUpdateCheckCompletions()
          << " Expected update check completions: "
          << expected_update_check_completions;
      return false;
    }

    return true;
  }

  // Sets a daily update check policy and returns true iff it's scheduled
  // correctly. |hours_from_now| must be > 0.
  bool CheckDailyUpdateCheck(int hours_fom_now) {
    DCHECK_GT(hours_fom_now, 0);
    // Calculate time from one hour from now and set the update check policy to
    // happen daily at that time.
    base::TimeDelta delay_from_now = base::TimeDelta::FromHours(hours_fom_now);
    auto policy_and_exploded_update_check_time = CreatePolicy(
        delay_from_now, DeviceScheduledUpdateChecker::Frequency::kDaily);

    // Set a new scheduled update setting, fast forward to right before the
    // expected update and then check if an update check is not scheduled.
    const base::TimeDelta small_delay = base::TimeDelta::FromMilliseconds(1);
    cros_settings_.device_settings()->Set(
        chromeos::kDeviceScheduledUpdateCheck,
        std::move(policy_and_exploded_update_check_time.first));
    int expected_update_checks =
        device_scheduled_update_checker_->GetUpdateCheckTimerExpirations();
    int expected_update_check_requests =
        fake_update_engine_client_->request_update_check_call_count();
    int expected_update_check_completions =
        device_scheduled_update_checker_->GetUpdateCheckCompletions();
    scoped_task_environment_.FastForwardBy(delay_from_now - small_delay);
    if (!CheckStats(expected_update_checks, expected_update_check_requests,
                    expected_update_check_completions)) {
      return false;
    }

    // Fast forward to the expected update check time and then check if the
    // update check is scheduled.
    expected_update_checks += 1;
    expected_update_check_requests += 1;
    expected_update_check_completions += 1;
    scoped_task_environment_.FastForwardBy(small_delay);

    // Simulate update check succeeding.
    NotifyUpdateCheckStatus(
        chromeos::UpdateEngineClient::UpdateStatusOperation::
            UPDATE_STATUS_UPDATED_NEED_REBOOT);
    if (!CheckStats(expected_update_checks, expected_update_check_requests,
                    expected_update_check_completions)) {
      return false;
    }

    // An update check should happen every day since the policy is set to daily.
    const int days = 5;
    for (int i = 0; i < days; i++) {
      expected_update_checks += 1;
      expected_update_check_requests += 1;
      expected_update_check_completions += 1;
      scoped_task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

      // Simulate update check succeeding.
      NotifyUpdateCheckStatus(
          chromeos::UpdateEngineClient::UpdateStatusOperation::
              UPDATE_STATUS_UPDATED_NEED_REBOOT);
      if (!CheckStats(expected_update_checks, expected_update_check_requests,
                      expected_update_check_completions)) {
        return false;
      }
    }

    return true;
  }

  // Creates an update check policy starting at a delay of |delta| from now and
  // recurring with frequency |frequency|.
  std::pair<base::Value, base::Time::Exploded> CreatePolicy(
      base::TimeDelta delta,
      DeviceScheduledUpdateChecker::Frequency frequency) {
    // Calculate time from one hour from now and set the update check policy to
    // happen daily at that time.
    base::Time update_check_time;
    base::Time::Exploded update_check_exploded_time;
    IncrementTimeByHours(scoped_task_environment_.GetMockClock()->Now(), delta,
                         &update_check_time, &update_check_exploded_time);

    base::Value scheduled_update_check_value;
    switch (frequency) {
      case DeviceScheduledUpdateChecker::Frequency::kDaily: {
        DecodeJsonStringAndNormalize(CreateDailyScheduledUpdateCheckPolicyJson(
                                         update_check_exploded_time.hour,
                                         update_check_exploded_time.minute),
                                     &scheduled_update_check_value);
        break;
      }

      case DeviceScheduledUpdateChecker::Frequency::kWeekly: {
        DecodeJsonStringAndNormalize(
            CreateWeeklyScheduledUpdateCheckPolicyJson(
                update_check_exploded_time.hour,
                update_check_exploded_time.minute,
                ExplodedTimeDayOfWeekToStringDayOfWeek(
                    update_check_exploded_time.day_of_week)),
            &scheduled_update_check_value);
        break;
      }

      case DeviceScheduledUpdateChecker::Frequency::kMonthly: {
        DecodeJsonStringAndNormalize(
            CreateMonthlyScheduledUpdateCheckPolicyJson(
                update_check_exploded_time.hour,
                update_check_exploded_time.minute,
                update_check_exploded_time.day_of_month),
            &scheduled_update_check_value);
        break;
      }
    }
    return std::make_pair(std::move(scheduled_update_check_value),
                          std::move(update_check_exploded_time));
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<DeviceScheduledUpdateCheckerForTest>
      device_scheduled_update_checker_;
  chromeos::ScopedTestingCrosSettings cros_settings_;
  chromeos::FakeUpdateEngineClient* fake_update_engine_client_;

 private:
  chromeos::ScopedStubInstallAttributes test_install_attributes_{
      chromeos::StubInstallAttributes::CreateCloudManaged("fake-domain",
                                                          "fake-id")};

  DISALLOW_COPY_AND_ASSIGN(DeviceScheduledUpdateCheckerTest);
};

TEST_F(DeviceScheduledUpdateCheckerTest, CheckIfDailyUpdateCheckIsScheduled) {
  // Check if back to back policies succeed.
  for (int i = 1; i <= 10; i++)
    EXPECT_TRUE(CheckDailyUpdateCheck(i));
}

TEST_F(DeviceScheduledUpdateCheckerTest, CheckIfWeeklyUpdateCheckIsScheduled) {
  // Set the first update check to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then weekly after.
  base::TimeDelta delay_from_now = base::TimeDelta::FromHours(49);
  auto policy_and_exploded_update_check_time = CreatePolicy(
      delay_from_now, DeviceScheduledUpdateChecker::Frequency::kWeekly);

  // Set a new scheduled update setting, fast forward to right before the
  // expected update and then check if an update check is not scheduled.
  int expected_update_checks = 0;
  int expected_update_check_requests = 0;
  int expected_update_check_completions = 0;
  const base::TimeDelta small_delay = base::TimeDelta::FromMilliseconds(1);
  cros_settings_.device_settings()->Set(
      chromeos::kDeviceScheduledUpdateCheck,
      std::move(policy_and_exploded_update_check_time.first));
  scoped_task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Fast forward to the expected update check time and then check if the update
  // check is scheduled.
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  scoped_task_environment_.FastForwardBy(small_delay);
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(chromeos::UpdateEngineClient::UpdateStatusOperation::
                              UPDATE_STATUS_UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // An update check should happen weekly since the policy is set to weekly.
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromDays(7));
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(chromeos::UpdateEngineClient::UpdateStatusOperation::
                              UPDATE_STATUS_UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

TEST_F(DeviceScheduledUpdateCheckerTest, CheckIfMonthlyUpdateCheckIsScheduled) {
  // Set the first update check to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then monthly after.
  base::TimeDelta delay_from_now = base::TimeDelta::FromHours(49);
  auto policy_and_exploded_update_check_time = CreatePolicy(
      delay_from_now, DeviceScheduledUpdateChecker::Frequency::kMonthly);

  // Set a new scheduled update setting, fast forward to right before the
  // expected update and then check if an update check is not scheduled.
  int expected_update_checks = 0;
  int expected_update_check_requests = 0;
  int expected_update_check_completions = 0;
  const base::TimeDelta small_delay = base::TimeDelta::FromMilliseconds(1);
  cros_settings_.device_settings()->Set(
      chromeos::kDeviceScheduledUpdateCheck,
      std::move(policy_and_exploded_update_check_time.first));
  scoped_task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Fast forward to the expected update check time and then check if the update
  // check is scheduled.
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  scoped_task_environment_.FastForwardBy(small_delay);
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(chromeos::UpdateEngineClient::UpdateStatusOperation::
                              UPDATE_STATUS_UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // The next update check should happen at the same day of month next month.
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  base::Optional<base::Time> next_check_time =
      update_checker_internal::IncrementMonthAndSetDayOfMonth(
          policy_and_exploded_update_check_time.second,
          policy_and_exploded_update_check_time.second.day_of_month);
  // This should be always set in a virtual time environment.
  EXPECT_TRUE(next_check_time);
  delay_from_now =
      next_check_time.value() - scoped_task_environment_.GetMockClock()->Now();
  base::Time::Exploded next_check_exploded_time;
  next_check_time->LocalExplode(&next_check_exploded_time);
  scoped_task_environment_.FastForwardBy(delay_from_now);
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(chromeos::UpdateEngineClient::UpdateStatusOperation::
                              UPDATE_STATUS_UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
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
  base::TimeDelta delay_from_now = base::TimeDelta::FromHours(1);
  auto policy_and_exploded_update_check_time = CreatePolicy(
      delay_from_now, DeviceScheduledUpdateChecker::Frequency::kDaily);

  // Fast forward time by less than (max retries * retry period) and check that
  // no update has occurred due to failure being simulated.
  int expected_update_checks = 0;
  int expected_update_check_requests = 0;
  int expected_update_check_completions = 0;
  cros_settings_.device_settings()->Set(
      chromeos::kDeviceScheduledUpdateCheck,
      std::move(policy_and_exploded_update_check_time.first));
  scoped_task_environment_.FastForwardBy(
      (update_checker_internal::kMaxStartUpdateCheckTimerRetryIterations - 2) *
      update_checker_internal::kStartUpdateCheckTimerRetryTime);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Reset failure mode and fast forward by the retry period. This time it
  // should succeed in setting an update check timer. No update checks should
  // happen yet but a check has just been scheduled.
  device_scheduled_update_checker_->SimulateCalculateNextUpdateCheckFailure(
      false);
  scoped_task_environment_.FastForwardBy(
      update_checker_internal::kStartUpdateCheckTimerRetryTime);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Check if update checks happen daily from now on.
  const int days = 2;
  for (int i = 0; i < days; i++) {
    expected_update_checks += 1;
    expected_update_check_requests += 1;
    expected_update_check_completions += 1;
    scoped_task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
    // Simulate update check succeeding.
    NotifyUpdateCheckStatus(
        chromeos::UpdateEngineClient::UpdateStatusOperation::
            UPDATE_STATUS_UPDATED_NEED_REBOOT);
    EXPECT_TRUE(CheckStats(expected_update_checks,
                           expected_update_check_requests,
                           expected_update_check_completions));
  }
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
      update_checker_internal::kMaxStartUpdateCheckTimerRetryIterations *
      update_checker_internal::kStartUpdateCheckTimerRetryTime);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateCheckTimerExpirations(),
            0);
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 0);

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
      update_checker_internal::kMaxStartUpdateCheckTimerRetryIterations *
      update_checker_internal::kStartUpdateCheckTimerRetryTime);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateCheckTimerExpirations(),
            0);

  // At this point all state has been reset. Reset failure mode and check if
  // daily update checks happen.
  chromeos::FakePowerManagerClient::Get()->simulate_start_arc_timer_failure(
      false);
  EXPECT_TRUE(CheckDailyUpdateCheck(1 /* hours_from_now */));
}

// Checks when an update check is unsuccessful retries are scheduled.
TEST_F(DeviceScheduledUpdateCheckerTest, CheckRetryLogicUpdateCheckFailure) {
  // Set the first update check to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then weekly after.
  base::TimeDelta delay_from_now = base::TimeDelta::FromHours(1);
  auto policy_and_exploded_update_check_time = CreatePolicy(
      delay_from_now, DeviceScheduledUpdateChecker::Frequency::kWeekly);

  // Set a new scheduled update setting, fast forward to expected update check
  // time and check if it happpens. Update check completion shouldn't happen as
  // an error is simulated.
  cros_settings_.device_settings()->Set(
      chromeos::kDeviceScheduledUpdateCheck,
      std::move(policy_and_exploded_update_check_time.first));
  int expected_update_checks = 1;
  int expected_update_check_requests = 1;
  int expected_update_check_completions = 0;
  scoped_task_environment_.FastForwardBy(delay_from_now);
  NotifyUpdateCheckStatus(
      chromeos::UpdateEngineClient::UpdateStatusOperation::UPDATE_STATUS_ERROR);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Fast forward for (max retries allowed) and check if each retry increases
  // the update check requests while we simulate an error.
  for (int i = 0;
       i <
       update_checker_internal::kMaxOsAndPoliciesUpdateCheckerRetryIterations;
       i++) {
    expected_update_check_requests += 1;
    scoped_task_environment_.FastForwardBy(
        update_checker_internal::kOsAndPoliciesUpdateCheckerRetryTime);
    // Simulate update check failing.
    NotifyUpdateCheckStatus(chromeos::UpdateEngineClient::
                                UpdateStatusOperation::UPDATE_STATUS_ERROR);
    EXPECT_TRUE(CheckStats(expected_update_checks,
                           expected_update_check_requests,
                           expected_update_check_completions));
  }

  // No retries should be scheduled till the next update check timer fires. Fast
  // forward to just before the timer firing and check.
  const base::TimeDelta delay_till_next_update_check_timer =
      base::TimeDelta::FromDays(update_checker_internal::kDaysInAWeek) -
      (update_checker_internal::kMaxOsAndPoliciesUpdateCheckerRetryIterations *
       update_checker_internal::kOsAndPoliciesUpdateCheckerRetryTime);
  const base::TimeDelta small_delay = base::TimeDelta::FromMilliseconds(1);
  scoped_task_environment_.FastForwardBy(delay_till_next_update_check_timer -
                                         small_delay);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Check if the next update check timer fires and an update check is
  // initiated.
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  scoped_task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

// Checks if an update check is successful after retries.
TEST_F(DeviceScheduledUpdateCheckerTest,
       CheckUpdateCheckFailureEventualSuccess) {
  // Set the first update check to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then weekly after.
  base::TimeDelta delay_from_now = base::TimeDelta::FromHours(49);
  auto policy_and_exploded_update_check_time = CreatePolicy(
      delay_from_now, DeviceScheduledUpdateChecker::Frequency::kWeekly);

  // Set a new scheduled update setting, fast forward to expected update check
  // time and check if it happpens. Update check completion shouldn't happen as
  // an error is simulated.
  cros_settings_.device_settings()->Set(
      chromeos::kDeviceScheduledUpdateCheck,
      std::move(policy_and_exploded_update_check_time.first));
  int expected_update_checks = 1;
  int expected_update_check_requests = 1;
  int expected_update_check_completions = 0;
  scoped_task_environment_.FastForwardBy(delay_from_now);
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(
      chromeos::UpdateEngineClient::UpdateStatusOperation::UPDATE_STATUS_ERROR);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Fast forward for (max retries allowed - 1) and check if each retry
  // increases the update check requests while we simulate an error.
  for (int i = 0;
       i <
       (update_checker_internal::kMaxOsAndPoliciesUpdateCheckerRetryIterations -
        1);
       i++) {
    expected_update_check_requests += 1;
    scoped_task_environment_.FastForwardBy(
        update_checker_internal::kOsAndPoliciesUpdateCheckerRetryTime);
    NotifyUpdateCheckStatus(chromeos::UpdateEngineClient::
                                UpdateStatusOperation::UPDATE_STATUS_ERROR);
    EXPECT_TRUE(CheckStats(expected_update_checks,
                           expected_update_check_requests,
                           expected_update_check_completions));
  }

  // Simulate success on the last retry attempt. This time the update check
  // should complete.
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  scoped_task_environment_.FastForwardBy(
      update_checker_internal::kOsAndPoliciesUpdateCheckerRetryTime);
  NotifyUpdateCheckStatus(chromeos::UpdateEngineClient::UpdateStatusOperation::
                              UPDATE_STATUS_UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

// Checks if an update check timer can't be started, any previous pending update
// completion will still be completed.
TEST_F(DeviceScheduledUpdateCheckerTest,
       CheckPreviousUpdateCheckCompletionWithTimerStartFailure) {
  // Calculate time from one hour from now and set the update check policy to
  // happen daily at that time.
  base::TimeDelta delay_from_now = base::TimeDelta::FromHours(1);
  auto policy_and_exploded_update_check_time = CreatePolicy(
      delay_from_now, DeviceScheduledUpdateChecker::Frequency::kDaily);

  // Set a new scheduled update setting, fast forward to the expected time and
  // and then check if an update check is scheduled.
  cros_settings_.device_settings()->Set(
      chromeos::kDeviceScheduledUpdateCheck,
      std::move(policy_and_exploded_update_check_time.first));
  int expected_update_checks = 1;
  int expected_update_check_requests = 1;
  int expected_update_check_completions = 0;
  scoped_task_environment_.FastForwardBy(delay_from_now);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Simulate timer start failure for the next time.
  chromeos::FakePowerManagerClient::Get()->simulate_start_arc_timer_failure(
      true);

  // Set a new scheduled update setting, this will fail to schedule the next
  // update check timer. However, it should still allow the previous update
  // check to finish.
  expected_update_check_completions += 1;
  delay_from_now = base::TimeDelta::FromHours(2);
  policy_and_exploded_update_check_time = CreatePolicy(
      delay_from_now, DeviceScheduledUpdateChecker::Frequency::kDaily);
  cros_settings_.device_settings()->Set(
      chromeos::kDeviceScheduledUpdateCheck,
      std::move(policy_and_exploded_update_check_time.first));
  scoped_task_environment_.FastForwardBy(
      update_checker_internal::kMaxStartUpdateCheckTimerRetryIterations *
      update_checker_internal::kStartUpdateCheckTimerRetryTime);
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(chromeos::UpdateEngineClient::UpdateStatusOperation::
                              UPDATE_STATUS_UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

}  // namespace policy
