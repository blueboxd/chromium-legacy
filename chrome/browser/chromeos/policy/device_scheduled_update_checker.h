// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_SCHEDULED_UPDATE_CHECKER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_SCHEDULED_UPDATE_CHECKER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/os_and_policies_update_checker.h"
#include "chrome/browser/chromeos/policy/task_executor_with_retries.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/dbus/power/native_timer.h"

namespace policy {

namespace update_checker_internal {

// Increments month (taking care of year rollovers) and sets day_of_month field
// to |month| in |local_exploded_time| and returns the corresponding base::Time.
// |local_exploded_time| should be local time. In case of an error, due to
// concurrent DST or time zone change, returns base::nullopt.
base::Optional<base::Time> IncrementMonthAndSetDayOfMonth(
    base::Time::Exploded exploded_time,
    int day_of_month);

// The maximum iterations allowed to start an update check timer if the
// operation fails.
constexpr int kMaxStartUpdateCheckTimerRetryIterations = 5;

// Time to call |StartUpdateCheckTimer| again in case it failed.
constexpr base::TimeDelta kStartUpdateCheckTimerRetryTime =
    base::TimeDelta::FromMinutes(5);

// Number of days in a week.
constexpr int kDaysInAWeek = 7;

}  // namespace update_checker_internal

// This class listens for changes in the scheduled update check policy and then
// manages recurring update checks based on the policy.
class DeviceScheduledUpdateChecker {
 public:
  explicit DeviceScheduledUpdateChecker(chromeos::CrosSettings* cros_settings);
  virtual ~DeviceScheduledUpdateChecker();

  // Frequency at which the update check should occur.
  enum class Frequency {
    kDaily,
    kWeekly,
    kMonthly,
  };

  // Holds the data associated with the current scheduled update check policy.
  struct ScheduledUpdateCheckData {
    ScheduledUpdateCheckData();
    ScheduledUpdateCheckData(const ScheduledUpdateCheckData&);
    ~ScheduledUpdateCheckData();

    int hour;

    int minute;

    Frequency frequency;

    // Only set when frequency is |kWeekly|. Corresponds to day_of_week in
    // base::Time::Exploded. Values between 0 (SUNDAY) to 6 (SATURDAY).
    base::Optional<int> day_of_week;

    // Only set when frequency is |kMonthly|. Corresponds to day_of_month in
    // base::Time::Exploded i.e. values between 1 to 28.
    base::Optional<int> day_of_month;

    // Absolute time ticks when the next update check (i.e. |UpdateCheck|) will
    // happen.
    base::TimeTicks next_update_check_time_ticks;
  };

 protected:
  // Called when |update_check_timer_| fires. Triggers an update check and
  // schedules the next update check based on |scheduled_update_check_data_|.
  virtual void OnUpdateCheckTimerExpired();

  // Calculates next update check time based on |scheduled_update_check_data_|
  // and |cur_local_time|. Returns |base::nullopt| if calculation failed due to
  // a concurrent DST or Time Zone change. Requires
  // |scheduled_update_check_data_| to be set.
  virtual base::Optional<base::Time> CalculateNextUpdateCheckTime(
      base::Time cur_local_time);

  // Called when |os_and_policies_update_checker_| has finished successfully or
  // unsuccessfully after retrying.
  virtual void OnUpdateCheckCompletion(bool result);

 private:
  // Callback triggered when scheduled update check setting has changed.
  void OnScheduledUpdateCheckDataChanged();

  // Must only be run via |start_update_check_timer_task_executor_|. Sets
  // |update_check_timer_| based on |scheduled_update_check_data_|. If the
  // |update_check_timer_| can't be started due to an error in
  // |CalculateNextUpdateCheckTime| then reschedules itself via
  // |start_update_check_timer_task_executor_|. Requires
  // |scheduled_update_check_data_| to be set.
  void StartUpdateCheckTimer();

  // Called upon starting |update_check_timer_|. Indicates whether or not the
  // timer was started successfully.
  void OnTimerStartResult(bool result);

  // Called when |start_update_check_timer_task_executor_|'s retry limit has
  // been reached.
  void OnStartUpdateCheckTimerRetryFailure();

  // Starts |start_update_check_timer_task_executor_| to run the next update
  // check timer, via |StartUpdateCheckTimer|, only if a policy i.e.
  // |scheduled_update_check_data_| is set.
  void MaybeStartUpdateCheckTimer();

  // Reset all state and cancel all pending tasks
  void ResetState();

  // Returns current time.
  virtual base::Time GetCurrentTime();

  // Returns time ticks from boot including time ticks spent during sleeping.
  virtual base::TimeTicks GetTicksSinceBoot();

  // Used to retrieve Chrome OS settings. Not owned.
  chromeos::CrosSettings* const cros_settings_;

  // Used to observe when settings change.
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      cros_settings_observer_;

  // Currently active scheduled update check policy.
  base::Optional<ScheduledUpdateCheckData> scheduled_update_check_data_;

  // Used to run and retry |StartUpdateCheckTimer| if it fails.
  TaskExecutorWithRetries start_update_check_timer_task_executor_;

  // Used to initiate update checks when |update_check_timer_| fires.
  OsAndPoliciesUpdateChecker os_and_policies_update_checker_;

  // Timer that is scheduled to check for updates.
  std::unique_ptr<chromeos::NativeTimer> update_check_timer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceScheduledUpdateChecker);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_SCHEDULED_UPDATE_CHECKER_H_
