// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_scheduled_update_checker.h"

#include <time.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/settings/cros_settings_names.h"

namespace policy {

namespace update_checker_internal {

base::Optional<base::Time> IncrementMonthAndSetDayOfMonth(
    base::Time::Exploded local_exploded_time,
    int day_of_month) {
  ++local_exploded_time.month;
  if (local_exploded_time.month == 13) {
    local_exploded_time.month = 1;
    ++local_exploded_time.year;
  }
  local_exploded_time.day_of_month = day_of_month;
  DCHECK(local_exploded_time.HasValidValues());

  // This can fail if there is a concurrent DST or time zone change.
  base::Time time;
  if (!base::Time::FromLocalExploded(local_exploded_time, &time))
    return base::nullopt;
  return time;
}

}  // namespace update_checker_internal

namespace {

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

// Convert the string day of week to an integer value suitable for
// |base::Time::Exploded::day_of_week|.
int StringDayOfWeekToExplodedTimeDayOfWeek(const std::string& day_of_week) {
  if (day_of_week == "SUNDAY")
    return 0;
  if (day_of_week == "MONDAY")
    return 1;
  if (day_of_week == "TUESDAY")
    return 2;
  if (day_of_week == "WEDNESDAY")
    return 3;
  if (day_of_week == "THURSDAY")
    return 4;
  if (day_of_week == "FRIDAY")
    return 5;
  DCHECK_EQ(day_of_week, "SATURDAY");
  return 6;
}

// Parses |value| into a |ScheduledUpdateCheckData|. Returns nullopt if there
// is any error while parsing |value|.
base::Optional<DeviceScheduledUpdateChecker::ScheduledUpdateCheckData>
ParseScheduledUpdate(const base::Value* value) {
  DeviceScheduledUpdateChecker::ScheduledUpdateCheckData result;
  // Parse mandatory values first i.e. hour, minute and frequency of update
  // check. These should always be present due to schema validation at higher
  // layers.
  const base::Value* hour_value = value->FindPathOfType(
      {"update_check_time", "hour"}, base::Value::Type::INTEGER);
  DCHECK(hour_value);
  int hour = hour_value->GetInt();
  // Validated by schema validation at higher layers.
  DCHECK(hour >= 0 && hour <= 23);
  result.hour = hour;

  const base::Value* minute_value = value->FindPathOfType(
      {"update_check_time", "minute"}, base::Value::Type::INTEGER);
  DCHECK(minute_value);
  int minute = minute_value->GetInt();
  // Validated by schema validation at higher layers.
  DCHECK(minute >= 0 && minute <= 59);
  result.minute = minute;

  // Validated by schema validation at higher layers.
  const std::string* frequency = value->FindStringKey({"frequency"});
  DCHECK(frequency);
  result.frequency = GetFrequency(*frequency);

  // Parse extra fields for weekly and monthly frequencies.
  switch (result.frequency) {
    case DeviceScheduledUpdateChecker::Frequency::kDaily:
      break;

    case DeviceScheduledUpdateChecker::Frequency::kWeekly: {
      const std::string* day_of_week = value->FindStringKey({"day_of_week"});
      if (!day_of_week) {
        LOG(ERROR) << "Day of week missing";
        return base::nullopt;
      }

      // Validated by schema validation at higher layers.
      result.day_of_week = StringDayOfWeekToExplodedTimeDayOfWeek(*day_of_week);
      break;
    }

    case DeviceScheduledUpdateChecker::Frequency::kMonthly: {
      base::Optional<int> day_of_month = value->FindIntKey({"day_of_month"});
      if (!day_of_month) {
        LOG(ERROR) << "Day of month missing";
        return base::nullopt;
      }

      // Validated by schema validation at higher layers.
      // TODO(crbug.com/924762): Currently |day_of_month| is restricted to 28 to
      // make month rollover calculations easier. Fix it to be smart enough to
      // handle all 31 days and month and year rollovers.
      DCHECK_LE(day_of_month.value(), 28);
      result.day_of_month = day_of_month.value();
      break;
    }
  }

  return result;
}

// Calculates the next update check time in a weekly policy. This function
// assumes the local time uses |kDaysInAWeek| days in a week.
base::Optional<base::Time> CalculateNextUpdateCheckWeeklyTime(
    const base::Time cur_time,
    int hour,
    int minute,
    int day_of_week) {
  // Calculate delay to get to next |day_of_week|.
  base::Time::Exploded cur_local_exploded_time;
  cur_time.LocalExplode(&cur_local_exploded_time);
  base::TimeDelta delay_from_cur;
  if (day_of_week < cur_local_exploded_time.day_of_week) {
    delay_from_cur = base::TimeDelta::FromDays(
        day_of_week + update_checker_internal::kDaysInAWeek -
        cur_local_exploded_time.day_of_week);
  } else {
    delay_from_cur = base::TimeDelta::FromDays(
        day_of_week - cur_local_exploded_time.day_of_week);
  }

  base::Time update_check_time = cur_time + delay_from_cur;
  base::Time::Exploded update_check_local_exploded_time;
  update_check_time.LocalExplode(&update_check_local_exploded_time);
  update_check_local_exploded_time.hour = hour;
  update_check_local_exploded_time.minute = minute;
  // This can fail if the timezone changed and the exploded time isn't valid
  // anymore i.e. a time 01:30 is not valid if the local time jumped from
  // 01:00 to 02:00.
  if (!base::Time::FromLocalExploded(update_check_local_exploded_time,
                                     &update_check_time)) {
    return base::nullopt;
  }

  // The greater than case can happen if the update is supposed to happen today
  // but at an earlier hour i.e today is Sunday 9 AM and the update check is
  // supposed to happen at Sunday 8 AM. The equal to case can happen when a
  // policy is set for today at the exact same time or when |UpdateCheck| calls
  // |StartUpdateCheckTimer|. In both cases advance the time to the next weekly
  // time.
  //
  // TODO(crbug.com/924762): A DST or TZ change can occur right before the call
  // to calculate |update_check_time| above which would make this next condition
  // correct. Add observers for both those changes and then recalculate update
  // check time again.
  if (cur_time >= update_check_time)
    update_check_time +=
        base::TimeDelta::FromDays(update_checker_internal::kDaysInAWeek);

  return update_check_time;
}

// Calculates the next update check time in a monthly policy.
base::Optional<base::Time> CalculateNextUpdateCheckMonthlyTime(
    const base::Time cur_time,
    int hour,
    int minute,
    int day_of_month) {
  // Calculate delay to get to next |day_of_month|.
  base::Time::Exploded cur_local_exploded_time;
  cur_time.LocalExplode(&cur_local_exploded_time);

  base::Time update_check_time;
  if (day_of_month < cur_local_exploded_time.day_of_month) {
    base::Optional<base::Time> result =
        update_checker_internal::IncrementMonthAndSetDayOfMonth(
            cur_local_exploded_time, day_of_month);
    if (!result)
      return base::nullopt;
    update_check_time = result.value();
  } else {
    update_check_time =
        cur_time + base::TimeDelta::FromDays(
                       day_of_month - cur_local_exploded_time.day_of_month);
  }

  // Set hour and minute to get the final update check time.
  base::Time::Exploded update_check_local_exploded_time;
  update_check_time.LocalExplode(&update_check_local_exploded_time);
  update_check_local_exploded_time.hour = hour;
  update_check_local_exploded_time.minute = minute;
  if (!base::Time::FromLocalExploded(update_check_local_exploded_time,
                                     &update_check_time)) {
    return base::nullopt;
  }

  // The greater than case can happen if the update is supposed to happen today
  // but at an earlier hour i.e today is Sunday 9 AM and the update check is
  // supposed to happen at Sunday 8 AM. The equal to case can happen when a
  // policy is set for today at the exact same time or when |UpdateCheck| calls
  // |StartUpdateCheckTimer|. In both cases advance the time to the next monthly
  // time.
  if (cur_time >= update_check_time) {
    base::Optional<base::Time> result =
        update_checker_internal::IncrementMonthAndSetDayOfMonth(
            cur_local_exploded_time, day_of_month);
    if (!result)
      return base::nullopt;
    update_check_time = result.value();
  }

  return update_check_time;
}

}  // namespace

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
  // Check if policy already exists.
  OnScheduledUpdateCheckDataChanged();
}

DeviceScheduledUpdateChecker::~DeviceScheduledUpdateChecker() = default;

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
      ParseScheduledUpdate(value);
  if (!scheduled_update_check_data) {
    LOG(ERROR) << "Failed to parse policy";
    return;
  }

  // Policy has been updated, calculate and set |update_check_timer_| again.
  scheduled_update_check_data_ = std::move(scheduled_update_check_data);
  MaybeStartUpdateCheckTimer();
}

base::Optional<base::Time>
DeviceScheduledUpdateChecker::CalculateNextUpdateCheckTime(
    base::Time cur_time) {
  DCHECK(scheduled_update_check_data_);

  // In order to calculate the next update check time first get the current
  // time and then modify it based on the policy set.
  base::Time update_check_time;
  switch (scheduled_update_check_data_->frequency) {
    case DeviceScheduledUpdateChecker::Frequency::kDaily: {
      base::Time::Exploded update_check_local_exploded_time;
      cur_time.LocalExplode(&update_check_local_exploded_time);
      update_check_local_exploded_time.hour =
          scheduled_update_check_data_->hour;
      update_check_local_exploded_time.minute =
          scheduled_update_check_data_->minute;
      DCHECK(update_check_local_exploded_time.HasValidValues());
      // This can fail if the timezone changed and the exploded time isn't valid
      // anymore i.e. a time 01:30 is not valid if the local time jumped from
      // 01:00 to 02:00.
      if (!base::Time::FromLocalExploded(update_check_local_exploded_time,
                                         &update_check_time)) {
        LOG(ERROR) << "Failed to calculate next daily update check time";
        return base::nullopt;
      }

      // If the time has passed for today then set the same time for the next
      // day.
      //
      // TODO(crbug.com/924762): A DST or TZ change can occur right before the
      // call to calculate |update_check_time| above which would make this next
      // condition correct. Add observers for both those changes and then
      // recalculate update check time again.
      if (cur_time >= update_check_time)
        update_check_time += base::TimeDelta::FromDays(1);

      break;
    }

    case DeviceScheduledUpdateChecker::Frequency::kWeekly: {
      DCHECK(scheduled_update_check_data_->day_of_week);
      base::Optional<base::Time> result = CalculateNextUpdateCheckWeeklyTime(
          cur_time, scheduled_update_check_data_->hour,
          scheduled_update_check_data_->minute,
          scheduled_update_check_data_->day_of_week.value());
      if (!result) {
        LOG(ERROR) << "Failed to calculate next weekly update check time";
        return base::nullopt;
      }
      update_check_time = result.value();

      break;
    }

    case DeviceScheduledUpdateChecker::Frequency::kMonthly: {
      DCHECK(scheduled_update_check_data_->day_of_month);
      base::Optional<base::Time> result = CalculateNextUpdateCheckMonthlyTime(
          cur_time, scheduled_update_check_data_->hour,
          scheduled_update_check_data_->minute,
          scheduled_update_check_data_->day_of_month.value());
      if (!result) {
        LOG(ERROR) << "Failed to calculate next monthly update check time";
        return base::nullopt;
      }
      update_check_time = result.value();

      break;
    }
  }

  DCHECK_NE(update_check_time, base::Time());
  return update_check_time;
}

void DeviceScheduledUpdateChecker::StartUpdateCheckTimer() {
  DCHECK(scheduled_update_check_data_);

  // For accuracy of the next update check, capture current time as close to the
  // start of this function as possible.
  const base::TimeTicks cur_ticks = GetTicksSinceBoot();
  const base::Time cur_time = GetCurrentTime();

  // Calculate the next update check time. In case there is an error while
  // calculating, due to concurrent DST or Time Zone changes, then reschedule
  // this function and try to schedule the update check again. There should only
  // be one outstanding task to start the timer.
  base::Optional<base::Time> update_check_time =
      CalculateNextUpdateCheckTime(cur_time);
  if (!update_check_time) {
    start_update_check_timer_task_executor_.ScheduleRetry();
    return;
  }
  scheduled_update_check_data_->next_update_check_time_ticks =
      cur_ticks + (update_check_time.value() - cur_time);

  // The timer could be destroyed in |OnScheduledUpdateCheckDataChanged|.
  if (!update_check_timer_) {
    update_check_timer_ =
        std::make_unique<chromeos::NativeTimer>(kUpdateCheckTimerTag);
  }

  // |update_check_timer_| will be destroyed as part of this object and is
  // guaranteed to not run callbacks after its destruction. Therefore, it's safe
  // to use "this" while starting the timer.
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
  // Retrying has a limit. In the unlikely scenario this is met, let an existing
  // lingering update check from a previous iteration finish. Reset all other
  // state including any callbacks. The next update check can only happen when
  // a new policy comes in or Chrome is restarted.
  LOG(ERROR) << "Failed to start update check timer after all retries";
  ResetState();
}

void DeviceScheduledUpdateChecker::MaybeStartUpdateCheckTimer() {
  // No need to start the next update check timer if the policy has been
  // removed. This can happen if an update check was ongoing, a new policy came
  // in but failed to start the timer which reset all state in
  // |OnStartUpdateCheckTimerRetryFailure|.
  if (!scheduled_update_check_data_)
    return;

  // Safe to use |this| as |start_update_check_timer_task_executor_| is a member
  // of |this|.
  start_update_check_timer_task_executor_.Start(
      base::BindRepeating(&DeviceScheduledUpdateChecker::StartUpdateCheckTimer,
                          base::Unretained(this)),
      base::BindOnce(
          &DeviceScheduledUpdateChecker::OnStartUpdateCheckTimerRetryFailure,
          base::Unretained(this)));
}

void DeviceScheduledUpdateChecker::OnUpdateCheckCompletion(bool result) {
  // Start the next update check timer irrespective of the current update check
  // succeeding or not.
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

}  // namespace policy
