// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_SCHEDULED_UPDATE_CHECKER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_SCHEDULED_UPDATE_CHECKER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/os_and_policies_update_checker.h"
#include "chrome/browser/chromeos/policy/task_executor_with_retries.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/dbus/power/native_timer.h"
#include "chromeos/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

// This class listens for changes in the scheduled update check policy and then
// manages recurring update checks based on the policy.
class DeviceScheduledUpdateChecker
    : public chromeos::system::TimezoneSettings::Observer {
 public:
  explicit DeviceScheduledUpdateChecker(chromeos::CrosSettings* cros_settings);
  ~DeviceScheduledUpdateChecker() override;

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

    // Corresponds to UCAL_HOUR_OF_DAY in icu::Calendar.
    int hour;

    // Corresponds to UCAL_MINUTE in icu::Calendar.
    int minute;

    Frequency frequency;

    // Only set when frequency is |kWeekly|. Corresponds to UCAL_DAY_OF_WEEK in
    // icu::Calendar. Values between 1 (SUNDAY) to 7 (SATURDAY).
    base::Optional<int> day_of_week;

    // Only set when frequency is |kMonthly|. Corresponds to UCAL_DAY_OF_MONTH
    // in icu::Calendar i.e. values between 1 to 31.
    base::Optional<int> day_of_month;

    // Absolute time ticks when the next update check (i.e. |UpdateCheck|) will
    // happen.
    base::TimeTicks next_update_check_time_ticks;
  };

  // chromeos::system::TimezoneSettings::Observer implementation.
  void TimezoneChanged(const icu::TimeZone& time_zone) override;

 protected:
  // Called when |update_check_timer_| fires. Triggers an update check and
  // schedules the next update check based on |scheduled_update_check_data_|.
  virtual void OnUpdateCheckTimerExpired();

  // Calculates the delay from |cur_time| at which |update_check_timer_| should
  // run next. Returns 0 delay if the calculation failed due to a concurrent DST
  // or Time Zone change. Requires |scheduled_update_check_data_| to be set.
  virtual base::TimeDelta CalculateNextUpdateCheckTimerDelay(
      base::Time cur_time);

  // Called when |os_and_policies_update_checker_| has finished successfully or
  // unsuccessfully after retrying.
  virtual void OnUpdateCheckCompletion(bool result);

 private:
  // Callback triggered when scheduled update check setting has changed.
  void OnScheduledUpdateCheckDataChanged();

  // Must only be run via |start_update_check_timer_task_executor_|. Sets
  // |update_check_timer_| based on |scheduled_update_check_data_|. If the
  // |update_check_timer_| can't be started due to an error in
  // |CalculateNextUpdateCheckTimerDelay| then reschedules itself via
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

  // Returns the current time zone.
  virtual const icu::TimeZone& GetTimeZone();

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

namespace update_checker_internal {

// Parses |value| into a |ScheduledUpdateCheckData|. Returns nullopt if there
// is any error while parsing |value|.
base::Optional<DeviceScheduledUpdateChecker::ScheduledUpdateCheckData>
ParseScheduledUpdate(const base::Value& value);

// Converts an icu::Calendar to base::Time. Assumes |time| is valid.
base::Time IcuToBaseTime(const icu::Calendar& time);

// Calculates the difference in milliseconds of |a| - |b|. Caller has to ensure
// |a| >= |b|.
base::TimeDelta GetDiff(const icu::Calendar& a, const icu::Calendar& b);

// Converts |cur_time| to ICU time in the time zone |tz|.
std::unique_ptr<icu::Calendar> ConvertUtcToTzIcuTime(base::Time cur_time,
                                                     const icu::TimeZone& tz);

// Modifies |time| to the next monthly date at |hour|, |minute| and
// |day_of_month| if it is greater than equal to |hour|:|minute| at
// |day_of_month|. It takes into account variance in days in different months
// i.e. 31st Jan is forwarded to 28th Feb in non leap years. However, if
// |day_of_month| equals |time|'s day of month and the |hour|:|minute| is
// greater than the |hour|:|minute| of |time| then only the |hour| and |minute|
// in |time| is set. Returns false iff there is a failure setting any of the
// fields.
//
// Some examples -
// - hour - 9 minute - 0 day_of_month - 31 time - 31st Jan 08:00 1970. |time|
// will be 31st Jan 09:00 1970.
// - hour - 7 minute - 0 day_of_month - 31 time - 31st Jan 08:00 1970. |time|
// will be 28th Feb 07:00 1970.
bool SetNextMonthlyDate(int hour,
                        int minute,
                        int day_of_month,
                        icu::Calendar* time);

// The maximum iterations allowed to start an update check timer if the
// operation fails.
constexpr int kMaxStartUpdateCheckTimerRetryIterations = 5;

// Time to call |StartUpdateCheckTimer| again in case it failed.
constexpr base::TimeDelta kStartUpdateCheckTimerRetryTime =
    base::TimeDelta::FromMinutes(5);

// Number of days in a week.
constexpr int kDaysInAWeek = 7;

}  // namespace update_checker_internal

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_SCHEDULED_UPDATE_CHECKER_H_
