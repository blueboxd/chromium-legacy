// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_scheduled_update_checker.h"

#include <time.h>

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/ucal.h"

namespace policy {

namespace {

// Number of minutes in an hour.
static constexpr int kMinutesInHour = 60;

// The tag associated to register |update_check_timer_|.
constexpr char kUpdateCheckTimerTag[] = "DeviceScheduledUpdateChecker";

// The tag associated to register |start_update_check_timer_task_executor_|.
constexpr char kStartUpdateCheckTimerTaskRunnerTag[] =
    "StartUpdateCheckTimerTaskRunner";

DeviceScheduledUpdateChecker::Frequency GetFrequency(
    const std::string& frequency) {
  if (frequency == "DAILY")
    return DeviceScheduledUpdateChecker::Frequency::kDaily;

  if (frequency == "WEEKLY")
    return DeviceScheduledUpdateChecker::Frequency::kWeekly;

  DCHECK_EQ(frequency, "MONTHLY");
  return DeviceScheduledUpdateChecker::Frequency::kMonthly;
}

// Convert the string day of week to UCalendarDaysOfWeek.
UCalendarDaysOfWeek StringDayOfWeekToIcuDayOfWeek(
    const std::string& day_of_week) {
  if (day_of_week == "SUNDAY")
    return UCAL_SUNDAY;
  if (day_of_week == "MONDAY")
    return UCAL_MONDAY;
  if (day_of_week == "TUESDAY")
    return UCAL_TUESDAY;
  if (day_of_week == "WEDNESDAY")
    return UCAL_WEDNESDAY;
  if (day_of_week == "THURSDAY")
    return UCAL_THURSDAY;
  if (day_of_week == "FRIDAY")
    return UCAL_FRIDAY;
  DCHECK_EQ(day_of_week, "SATURDAY");
  return UCAL_SATURDAY;
}

// Returns true iff a >= b.
bool IsCalGreaterThanEqual(const icu::Calendar& a, const icu::Calendar& b) {
  UErrorCode status = U_ZERO_ERROR;
  if (a.after(b, status)) {
    DCHECK(U_SUCCESS(status));
    return true;
  }

  if (a.equals(b, status)) {
    DCHECK(U_SUCCESS(status));
    return true;
  }

  return false;
}

// Calculates the next update check timer delay from |cur_time| in a daily
// policy.
base::TimeDelta CalculateNextDailyUpdateCheckTimerDelay(
    base::Time cur_time,
    int hour,
    int minute,
    const icu::TimeZone& tz) {
  auto cur_cal = update_checker_internal::ConvertUtcToTzIcuTime(cur_time, tz);
  if (!cur_cal) {
    LOG(ERROR) << "Failed to calculate next daily update check time";
    return base::TimeDelta();
  }

  // Set the hour and minute in the current date to get the update check time.
  auto update_check_cal = base::WrapUnique(cur_cal->clone());
  DCHECK(update_check_cal);
  update_check_cal->set(UCAL_HOUR_OF_DAY, hour);
  update_check_cal->set(UCAL_MINUTE, minute);
  update_check_cal->set(UCAL_SECOND, 0);
  update_check_cal->set(UCAL_MILLISECOND, 0);

  // If the time has passed for today then set the same time for the next
  // day.
  UErrorCode status = U_ZERO_ERROR;
  if (IsCalGreaterThanEqual(*cur_cal, *update_check_cal)) {
    update_check_cal->add(UCAL_DAY_OF_MONTH, 1, status);
    if (U_FAILURE(status))
      return base::TimeDelta();
  }

  return update_checker_internal::GetDiff(*update_check_cal, *cur_cal);
}

// Calculates the next update check timer delay from |cur_time| in a weekly
// policy. This function assumes the local time uses |kDaysInAWeek| days in a
// week.
base::TimeDelta CalculateNextWeeklyUpdateCheckTimerDelay(
    base::Time cur_time,
    int hour,
    int minute,
    int day_of_week,
    const icu::TimeZone& tz) {
  auto cur_cal = update_checker_internal::ConvertUtcToTzIcuTime(cur_time, tz);
  if (!cur_cal) {
    LOG(ERROR) << "Failed to calculate next weekly update check time";
    return base::TimeDelta();
  }
  UErrorCode status = U_ZERO_ERROR;
  int cur_day_of_week = cur_cal->get(UCAL_DAY_OF_WEEK, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Failed to get day of week";
    return base::TimeDelta();
  }

  // Calculate days to get to next |day_of_week| by constructing the next update
  // check time.
  auto update_check_cal = base::WrapUnique(cur_cal->clone());
  DCHECK(update_check_cal);
  update_check_cal->set(UCAL_HOUR_OF_DAY, hour);
  update_check_cal->set(UCAL_MINUTE, minute);
  update_check_cal->set(UCAL_SECOND, 0);
  update_check_cal->set(UCAL_MILLISECOND, 0);

  // Add days required to get to |cur_day_of_week|.
  int days_to_go;
  if (day_of_week < cur_day_of_week) {
    days_to_go =
        day_of_week + update_checker_internal::kDaysInAWeek - cur_day_of_week;
  } else {
    days_to_go = day_of_week - cur_day_of_week;
  }
  update_check_cal->add(UCAL_DAY_OF_MONTH, days_to_go, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Failed to add num days: " << days_to_go;
    return base::TimeDelta();
  }

  // The greater than case can happen if the update is supposed to happen today
  // but at an earlier hour i.e today is Sunday 9 AM and the update check is
  // supposed to happen at Sunday 8 AM. The equal to case can happen when a
  // policy is set for today at the exact same time or when |UpdateCheck| calls
  // |StartUpdateCheckTimer|. In both cases advance the time to the next weekly
  // time.
  if (IsCalGreaterThanEqual(*cur_cal, *update_check_cal)) {
    update_check_cal->add(UCAL_DAY_OF_MONTH,
                          update_checker_internal::kDaysInAWeek, status);
    if (U_FAILURE(status)) {
      LOG(ERROR) << "Failed to add num days: "
                 << update_checker_internal::kDaysInAWeek;
      return base::TimeDelta();
    }
  }

  return update_checker_internal::GetDiff(*update_check_cal, *cur_cal);
}

// Calculates the next update check timer delay from |cur_time| in a monthly
// policy.
base::TimeDelta CalculateNextMonthlyUpdateCheckTimerDelay(
    base::Time cur_time,
    int hour,
    int minute,
    int day_of_month,
    const icu::TimeZone& tz) {
  // Calculate delay to get to next |day_of_month|.
  auto cur_cal = update_checker_internal::ConvertUtcToTzIcuTime(cur_time, tz);
  if (!cur_cal) {
    LOG(ERROR) << "Failed to calculate next monthly update check time";
    return base::TimeDelta();
  }

  auto update_check_cal = base::WrapUnique(cur_cal->clone());
  DCHECK(update_check_cal);
  if (!update_checker_internal::SetNextMonthlyDate(hour, minute, day_of_month,
                                                   update_check_cal.get())) {
    LOG(ERROR) << "Failed to increment month and set day of month";
    return base::TimeDelta();
  }

  return update_checker_internal::GetDiff(*update_check_cal, *cur_cal);
}

}  // namespace

namespace update_checker_internal {

base::Optional<DeviceScheduledUpdateChecker::ScheduledUpdateCheckData>
ParseScheduledUpdate(const base::Value& value) {
  DeviceScheduledUpdateChecker::ScheduledUpdateCheckData result;
  // Parse mandatory values first i.e. hour, minute and frequency of update
  // check. These should always be present due to schema validation at higher
  // layers.
  const base::Value* hour_value = value.FindPathOfType(
      {"update_check_time", "hour"}, base::Value::Type::INTEGER);
  DCHECK(hour_value);
  int hour = hour_value->GetInt();
  // Validated by schema validation at higher layers.
  DCHECK(hour >= 0 && hour <= 23);
  result.hour = hour;

  const base::Value* minute_value = value.FindPathOfType(
      {"update_check_time", "minute"}, base::Value::Type::INTEGER);
  DCHECK(minute_value);
  int minute = minute_value->GetInt();
  // Validated by schema validation at higher layers.
  DCHECK(minute >= 0 && minute <= 59);
  result.minute = minute;

  // Validated by schema validation at higher layers.
  const std::string* frequency = value.FindStringKey({"frequency"});
  DCHECK(frequency);
  result.frequency = GetFrequency(*frequency);

  // Parse extra fields for weekly and monthly frequencies.
  switch (result.frequency) {
    case DeviceScheduledUpdateChecker::Frequency::kDaily:
      break;

    case DeviceScheduledUpdateChecker::Frequency::kWeekly: {
      const std::string* day_of_week = value.FindStringKey({"day_of_week"});
      if (!day_of_week) {
        LOG(ERROR) << "Day of week missing";
        return base::nullopt;
      }

      // Validated by schema validation at higher layers.
      result.day_of_week = StringDayOfWeekToIcuDayOfWeek(*day_of_week);
      break;
    }

    case DeviceScheduledUpdateChecker::Frequency::kMonthly: {
      base::Optional<int> day_of_month = value.FindIntKey({"day_of_month"});
      if (!day_of_month) {
        LOG(ERROR) << "Day of month missing";
        return base::nullopt;
      }

      // Validated by schema validation at higher layers.
      // TODO(crbug.com/924762): Currently |day_of_month| is restricted to 28 to
      // make month rollover calculations easier. Fix it to be smart enough to
      // handle all 31 days and month and year rollovers.
      result.day_of_month = day_of_month.value();
      break;
    }
  }

  return result;
}

// Converts an icu::Calendar to base::Time. Assumes |time| is a valid time.
base::Time IcuToBaseTime(const icu::Calendar& time) {
  UErrorCode status = U_ZERO_ERROR;
  UDate seconds_from_epoch = time.getTime(status) / 1000;
  DCHECK(U_SUCCESS(status));
  base::Time result = base::Time::FromTimeT(seconds_from_epoch);
  if (result.is_null())
    result = base::Time::UnixEpoch();
  return result;
}

base::TimeDelta GetDiff(const icu::Calendar& a, const icu::Calendar& b) {
  UErrorCode status = U_ZERO_ERROR;
  UDate a_ms = a.getTime(status);
  DCHECK(U_SUCCESS(status));
  UDate b_ms = b.getTime(status);
  DCHECK(U_SUCCESS(status));
  DCHECK(a_ms >= b_ms);
  return base::TimeDelta::FromMilliseconds(a_ms - b_ms);
}

std::unique_ptr<icu::Calendar> ConvertUtcToTzIcuTime(base::Time cur_time,
                                                     const icu::TimeZone& tz) {
  // Get ms from epoch for |cur_time| and use it to get the new time in |tz|.
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> cal_tz =
      std::make_unique<icu::GregorianCalendar>(tz, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Couldn't create calendar";
    return nullptr;
  }
  // Erase current time from the calendar.
  cal_tz->clear();
  time_t ms_from_epoch = cur_time.ToTimeT() * 1000;
  cal_tz->setTime(ms_from_epoch, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Couldn't create calendar";
    return nullptr;
  }

  return cal_tz;
}

bool SetNextMonthlyDate(int hour,
                        int minute,
                        int day_of_month,
                        icu::Calendar* time) {
  DCHECK(time);

  UErrorCode status = U_ZERO_ERROR;
  int cur_month = time->get(UCAL_MONTH, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Failed to get month";
    return false;
  }

  int cur_day_of_month = time->get(UCAL_DAY_OF_MONTH, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Failed to get day of month";
    return false;
  }

  int cur_max_days_in_month = time->getActualMaximum(UCAL_DAY_OF_MONTH, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Failed to get max days in month";
    return false;
  }

  int cur_hour = time->get(UCAL_HOUR_OF_DAY, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Failed to get cur hour";
    return false;
  }

  int cur_minute = time->get(UCAL_MINUTE, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Failed to get cur minute";
    return false;
  }

  int mins_in_day = hour * kMinutesInHour + minute;
  int cur_mins_in_day = cur_hour * kMinutesInHour + cur_minute;

  // If |day_of_month| is < |cur_day_of_month| then the next time has to be next
  // month, hence add or roll a month to the current date.
  //
  // If its the same day of month and current time has passed |hour|:|month|
  // even then add or roll a month.
  //
  // If |day_of_month| > |cur_day_of_month| then it's definitely in the future
  // but needs to be capped to |cur_max_days_in_month|.
  if ((day_of_month < cur_day_of_month) ||
      ((day_of_month == cur_day_of_month) &&
       (mins_in_day <= cur_mins_in_day))) {
    // Roll guarantees that 31st Jan turns into 28th Feb (or 29th Feb in
    // non-leap years). However, on 31st December 1969 using roll will give 31st
    // January 1969 whereas 31st Jnauary 1970 was wanted. Hence, add is used
    // when the month is December.
    if (cur_month == UCAL_DECEMBER)
      time->add(UCAL_MONTH, 1, status);
    else
      time->roll(UCAL_MONTH, 1, status);
    if (U_FAILURE(status)) {
      LOG(ERROR) << "Failed to increment month";
      return false;
    }
  } else if (day_of_month > cur_max_days_in_month) {
    day_of_month = cur_max_days_in_month;
  }
  time->set(UCAL_DAY_OF_MONTH, day_of_month);
  time->set(UCAL_HOUR_OF_DAY, hour);
  time->set(UCAL_MINUTE, minute);
  time->set(UCAL_SECOND, 0);
  time->set(UCAL_MILLISECOND, 0);

  return true;
}

}  // namespace update_checker_internal

// |cros_settings_observer_| will be destroyed as part of this object
// guaranteeing to not run |OnScheduledUpdateCheckDataChanged| after its
// destruction. Therefore, it's safe to use "this" while adding this observer.
// Similarly, |start_update_check_timer_task_executor_| and
// |os_and_policies_update_checker_| will be destroyed as part of this object,
// so it's safe to use "this" with any callbacks.
DeviceScheduledUpdateChecker::DeviceScheduledUpdateChecker(
    chromeos::CrosSettings* cros_settings)
    : cros_settings_(cros_settings),
      cros_settings_observer_(cros_settings_->AddSettingsObserver(
          chromeos::kDeviceScheduledUpdateCheck,
          base::BindRepeating(
              &DeviceScheduledUpdateChecker::OnScheduledUpdateCheckDataChanged,
              base::Unretained(this)))),
      start_update_check_timer_task_executor_(
          kStartUpdateCheckTimerTaskRunnerTag,
          base::BindRepeating(&DeviceScheduledUpdateChecker::GetTicksSinceBoot,
                              base::Unretained(this)),
          update_checker_internal::kMaxStartUpdateCheckTimerRetryIterations,
          update_checker_internal::kStartUpdateCheckTimerRetryTime),
      os_and_policies_update_checker_(
          base::BindRepeating(&DeviceScheduledUpdateChecker::GetTicksSinceBoot,
                              base::Unretained(this))) {
  chromeos::system::TimezoneSettings::GetInstance()->AddObserver(this);
  // Check if policy already exists.
  OnScheduledUpdateCheckDataChanged();
}

DeviceScheduledUpdateChecker::~DeviceScheduledUpdateChecker() {
  chromeos::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

DeviceScheduledUpdateChecker::ScheduledUpdateCheckData::
    ScheduledUpdateCheckData() = default;
DeviceScheduledUpdateChecker::ScheduledUpdateCheckData::
    ScheduledUpdateCheckData(const ScheduledUpdateCheckData&) = default;
DeviceScheduledUpdateChecker::ScheduledUpdateCheckData::
    ~ScheduledUpdateCheckData() = default;

void DeviceScheduledUpdateChecker::OnUpdateCheckTimerExpired() {
  // If no policy exists, state should have been reset and this callback
  // shouldn't have fired.
  DCHECK(scheduled_update_check_data_);

  // Following things needs to be done on every update check event. These will
  // be done serially to make state management easier -
  // - Check for download any updates.
  // - TODO(crbug.com/924762)  Refresh policies.
  // - Calculate and start the next update check timer.
  //
  // |os_and_policies_update_checker_| will be destroyed as part of this object,
  // so it's safe to use "this" with any callbacks. This overrides any previous
  // update check calls. Since, the minimum update frequency is daily there is
  // very little chance of stepping on an existing update check that hasn't
  // finished for a day. Timeouts will ensure that an update check completes,
  // successfully or unsuccessfully, way before a day.
  os_and_policies_update_checker_.Start(
      base::BindOnce(&DeviceScheduledUpdateChecker::OnUpdateCheckCompletion,
                     base::Unretained(this)));
}

void DeviceScheduledUpdateChecker::TimezoneChanged(
    const icu::TimeZone& time_zone) {
  // Anytime the time zone changes, the update check timer delay should be
  // recalculated and the timer should be started with updated values according
  // to the new time zone.
  MaybeStartUpdateCheckTimer();
}

void DeviceScheduledUpdateChecker::OnScheduledUpdateCheckDataChanged() {
  // If the policy is removed then reset all state including any existing update
  // checks.
  const base::Value* value =
      cros_settings_->GetPref(chromeos::kDeviceScheduledUpdateCheck);
  if (!value) {
    ResetState();
    os_and_policies_update_checker_.Stop();
    return;
  }

  // Keep any old policy timers running if a new policy is ill-formed and can't
  // be used to set a new timer.
  base::Optional<ScheduledUpdateCheckData> scheduled_update_check_data =
      update_checker_internal::ParseScheduledUpdate(*value);
  if (!scheduled_update_check_data) {
    LOG(ERROR) << "Failed to parse policy";
    return;
  }

  // Policy has been updated, calculate and set |update_check_timer_| again.
  scheduled_update_check_data_ = std::move(scheduled_update_check_data);
  MaybeStartUpdateCheckTimer();
}

base::TimeDelta
DeviceScheduledUpdateChecker::CalculateNextUpdateCheckTimerDelay(
    base::Time cur_time) {
  DCHECK(scheduled_update_check_data_);

  // In order to calculate the next update check time first get the current
  // time and then modify it based on the policy set.
  switch (scheduled_update_check_data_->frequency) {
    case DeviceScheduledUpdateChecker::Frequency::kDaily: {
      return CalculateNextDailyUpdateCheckTimerDelay(
          cur_time, scheduled_update_check_data_->hour,
          scheduled_update_check_data_->minute, GetTimeZone());
    }

    case DeviceScheduledUpdateChecker::Frequency::kWeekly: {
      DCHECK(scheduled_update_check_data_->day_of_week);
      return CalculateNextWeeklyUpdateCheckTimerDelay(
          cur_time, scheduled_update_check_data_->hour,
          scheduled_update_check_data_->minute,
          scheduled_update_check_data_->day_of_week.value(), GetTimeZone());
    }

    case DeviceScheduledUpdateChecker::Frequency::kMonthly: {
      DCHECK(scheduled_update_check_data_->day_of_month);
      return CalculateNextMonthlyUpdateCheckTimerDelay(
          cur_time, scheduled_update_check_data_->hour,
          scheduled_update_check_data_->minute,
          scheduled_update_check_data_->day_of_month.value(), GetTimeZone());
    }
  }
}

void DeviceScheduledUpdateChecker::StartUpdateCheckTimer() {
  DCHECK(scheduled_update_check_data_);

  // For accuracy of the next update check, capture current time as close to
  // the start of this function as possible.
  const base::TimeTicks cur_ticks = GetTicksSinceBoot();
  const base::Time cur_time = GetCurrentTime();

  // Calculate the next update check time. In case there is an error while
  // calculating, due to concurrent DST or Time Zone changes, then reschedule
  // this function and try to schedule the update check again. There should only
  // be one outstanding task to start the timer.
  base::TimeDelta delay = CalculateNextUpdateCheckTimerDelay(cur_time);
  if (delay.is_zero()) {
    LOG(ERROR) << "Failed to calculate next update check time";
    start_update_check_timer_task_executor_.ScheduleRetry();
    return;
  }
  scheduled_update_check_data_->next_update_check_time_ticks =
      cur_ticks + delay;

  // The timer could be destroyed in |OnScheduledUpdateCheckDataChanged|.
  if (!update_check_timer_) {
    update_check_timer_ =
        std::make_unique<chromeos::NativeTimer>(kUpdateCheckTimerTag);
  }

  // |update_check_timer_| will be destroyed as part of this object and is
  // guaranteed to not run callbacks after its destruction. Therefore, it's
  // safe to use "this" while starting the timer.
  update_check_timer_->Start(
      scheduled_update_check_data_->next_update_check_time_ticks,
      base::BindOnce(&DeviceScheduledUpdateChecker::OnUpdateCheckTimerExpired,
                     base::Unretained(this)),
      base::BindOnce(&DeviceScheduledUpdateChecker::OnTimerStartResult,
                     base::Unretained(this)));
}

void DeviceScheduledUpdateChecker::OnTimerStartResult(bool result) {
  // Schedule a retry if |update_check_timer_| failed to start.
  if (!result) {
    LOG(ERROR) << "Failed to start update check timer";
    start_update_check_timer_task_executor_.ScheduleRetry();
    return;
  }
}

void DeviceScheduledUpdateChecker::OnStartUpdateCheckTimerRetryFailure() {
  // Retrying has a limit. In the unlikely scenario this is met, let an
  // existing lingering update check from a previous iteration finish. Reset
  // all other state including any callbacks. The next update check can only
  // happen when a new policy comes in or Chrome is restarted.
  LOG(ERROR) << "Failed to start update check timer after all retries";
  ResetState();
}

void DeviceScheduledUpdateChecker::MaybeStartUpdateCheckTimer() {
  // No need to start the next update check timer if the policy has been
  // removed. This can happen if an update check was ongoing, a new policy
  // came in but failed to start the timer which reset all state in
  // |OnStartUpdateCheckTimerRetryFailure|.
  if (!scheduled_update_check_data_)
    return;

  // Safe to use |this| as |start_update_check_timer_task_executor_| is a
  // member of |this|.
  start_update_check_timer_task_executor_.Start(
      base::BindRepeating(&DeviceScheduledUpdateChecker::StartUpdateCheckTimer,
                          base::Unretained(this)),
      base::BindOnce(
          &DeviceScheduledUpdateChecker::OnStartUpdateCheckTimerRetryFailure,
          base::Unretained(this)));
}

void DeviceScheduledUpdateChecker::OnUpdateCheckCompletion(bool result) {
  // Start the next update check timer irrespective of the current update
  // check succeeding or not.
  LOG_IF(ERROR, !result) << "Update check failed";
  MaybeStartUpdateCheckTimer();
}

void DeviceScheduledUpdateChecker::ResetState() {
  update_check_timer_.reset();
  scheduled_update_check_data_ = base::nullopt;
  start_update_check_timer_task_executor_.Stop();
}

base::Time DeviceScheduledUpdateChecker::GetCurrentTime() {
  return base::Time::Now();
}

base::TimeTicks DeviceScheduledUpdateChecker::GetTicksSinceBoot() {
  struct timespec ts = {};
  int ret = clock_gettime(CLOCK_BOOTTIME, &ts);
  DCHECK_NE(ret, 0);
  return base::TimeTicks() + base::TimeDelta::FromTimeSpec(ts);
}

const icu::TimeZone& DeviceScheduledUpdateChecker::GetTimeZone() {
  return chromeos::system::TimezoneSettings::GetInstance()->GetTimezone();
}

}  // namespace policy
