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
constexpr int kMaxRetryUpdateCheckIterations = 5;

// Time to call |StartUpdateCheckTimer| again in case it failed.
constexpr base::TimeDelta kStartUpdateCheckTimerRetryTime =
    base::TimeDelta::FromMinutes(5);

}  // namespace update_checker_internal

// This class listens for changes in the scheduled update check policy and then
// manages recurring update checks based on the policy.
class DeviceScheduledUpdateChecker {
 public:
  explicit DeviceScheduledUpdateChecker(chromeos::CrosSettings* cros_settings);
  virtual ~DeviceScheduledUpdateChecker();

  // Holds the data associated with the current scheduled update check policy.
  struct ScheduledUpdateCheckData {
    ScheduledUpdateCheckData();
    ScheduledUpdateCheckData(const ScheduledUpdateCheckData&);
    ~ScheduledUpdateCheckData();

    enum class Frequency {
      kDaily,
      kWeekly,
      kMonthly,
    };

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
  virtual void UpdateCheck();

  // Calculates next update check time based on |scheduled_update_check_data_|
  // and |cur_local_time|. Returns |base::nullopt| if calculation failed due to
  // a concurrent DST or Time Zone change. Requires
  // |scheduled_update_check_data_| to be set.
  virtual base::Optional<base::Time> CalculateNextUpdateCheckTime(
      base::Time cur_local_time);

 private:
  // Callback triggered when scheduled update check setting has changed.
  void OnScheduledUpdateCheckDataChanged();

  // Sets |update_check_timer_| based on |scheduled_update_check_data_|. If the
  // |update_check_timer_| can't be started due to an error in
  // |CalculateNextUpdateCheckTime| then reschedules itself to try again.
  // Requires |scheduled_update_check_data_| to be set.
  void StartUpdateCheckTimer();

  // Called upon starting |update_check_timer_| or
  // |start_update_check_retry_timer_|. Indicates whether or not the timer was
  // started successfully.
  void OnTimerStartResult(bool result);

  // Cancels any pending |StartUpdateCheckTimer| calls and reschedules it after
  // a delay.
  void RetryStartUpdateCheckTimer();

  // Reset all state and cancel all pending tasks.
  void ResetState();

  // Returns current time.
  virtual base::Time GetCurrentTime();

  // Returns time ticks from boot including time ticks spent during sleeping.
  virtual base::TimeTicks GetTicksSinceBoot();

  // The number of attempts for which |update_check_timer_| has tried to be
  // started.
  int update_check_timer_start_attempts_ = 0;

  // Used to retrieve Chrome OS settings. Not owned.
  chromeos::CrosSettings* const cros_settings_;

  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      cros_settings_observer_;

  // Currently active scheduled update check policy.
  base::Optional<ScheduledUpdateCheckData> scheduled_update_check_data_;

  // Timer that is scheduled to check for updates.
  std::unique_ptr<chromeos::NativeTimer> update_check_timer_;

  // Timer that retries |StartUpdateCheckTimer| in case it fails to start
  // |update_check_timer_|. This needs to be suspend aware as well because the
  // retry needs to be done before the time for the next update check and
  // between that time the device maybe suspended indefinitely. Consequently, if
  // this timer doesn't run in suspend then there is a chance to miss the next
  // update check.
  std::unique_ptr<chromeos::NativeTimer> start_update_check_retry_timer_;

  base::WeakPtrFactory<DeviceScheduledUpdateChecker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceScheduledUpdateChecker);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_SCHEDULED_UPDATE_CHECKER_H_
