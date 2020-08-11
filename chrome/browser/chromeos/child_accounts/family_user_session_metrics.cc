// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_session_metrics.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

constexpr int kEngagementHourBuckets = base::Time::kHoursPerDay;
constexpr base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);

// Returns the hour (0-23) within the day for given local time.
int HourOfDay(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return exploded.hour;
}

// Returns 0-based day of week (0 = Sunday, etc.)
int DayOfWeek(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return exploded.day_of_week;
}

}  // namespace

// static
const char FamilyUserSessionMetrics::kSessionEngagementStartActionName[] =
    "FamilyUser.SessionEngagement.Start";
const char
    FamilyUserSessionMetrics::kUserSessionEngagementWeekdayHistogramName[] =
        "FamilyUser.SessionEngagement.Weekday";
const char
    FamilyUserSessionMetrics::kUserSessionEngagementWeekendHistogramName[] =
        "FamilyUser.SessionEngagement.Weekend";
const char
    FamilyUserSessionMetrics::kUserSessionEngagementTotalHistogramName[] =
        "FamilyUser.SessionEngagement.Total";

// static
void FamilyUserSessionMetrics::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(
      prefs::kFamilyUserMetricsSessionEngagementStartTime, base::Time());
}

FamilyUserSessionMetrics::FamilyUserSessionMetrics(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  chromeos::UsageTimeStateNotifier::GetInstance()->AddObserver(this);
}

FamilyUserSessionMetrics::~FamilyUserSessionMetrics() {
  if (is_user_active_) {
    is_user_active_ = false;
    UpdateUserEngagement();
  }

  chromeos::UsageTimeStateNotifier::GetInstance()->RemoveObserver(this);
}

void FamilyUserSessionMetrics::OnUsageTimeStateChange(
    chromeos::UsageTimeStateNotifier::UsageTimeState state) {
  is_user_active_ =
      state == chromeos::UsageTimeStateNotifier::UsageTimeState::ACTIVE;

  UpdateUserEngagement();
}

void FamilyUserSessionMetrics::SaveSessionEngagementStartTime() {
  pref_service_->SetTime(prefs::kFamilyUserMetricsSessionEngagementStartTime,
                         base::Time::Now());
}

void FamilyUserSessionMetrics::ReportUserEngagementHourToUma(base::Time start,
                                                             base::Time end) {
  if (start.is_null() || end.is_null() || end < start)
    return;
  base::Time time = start;
  while (time <= end) {
    int day_of_week = DayOfWeek(time);
    int hour_of_day = HourOfDay(time);
    if (day_of_week == 0 || day_of_week == 6) {
      base::UmaHistogramExactLinear(kUserSessionEngagementWeekendHistogramName,
                                    hour_of_day, kEngagementHourBuckets);
    } else {
      base::UmaHistogramExactLinear(kUserSessionEngagementWeekdayHistogramName,
                                    hour_of_day, kEngagementHourBuckets);
    }

    base::UmaHistogramExactLinear(kUserSessionEngagementTotalHistogramName,
                                  hour_of_day, kEngagementHourBuckets);

    // When the difference between end and time less than 1 hour and their hours
    // of day are different, i.e. time = 10:55 and end = 11:05, we need to
    // report both 10 and 11. To ensure we don't omit reporting 11, set |time|
    // equal to |end|.
    if (end - time < kOneHour && hour_of_day != HourOfDay(end)) {
      time = end;
    } else {
      time += kOneHour;
    }
  }
}

void FamilyUserSessionMetrics::ReportSessionEngagementStartToUma() {
  base::RecordAction(
      base::UserMetricsAction(kSessionEngagementStartActionName));
}

void FamilyUserSessionMetrics::UpdateUserEngagement() {
  if (is_user_active_) {
    ReportSessionEngagementStartToUma();
    SaveSessionEngagementStartTime();
  } else {
    base::Time start = pref_service_->GetTime(
        prefs::kFamilyUserMetricsSessionEngagementStartTime);

    ReportUserEngagementHourToUma(
        /*start=*/start,
        /*end=*/base::Time::Now());

    ResetSessionEngagementStartPref();
  }
}

void FamilyUserSessionMetrics::ResetSessionEngagementStartPref() {
  pref_service_->ClearPref(prefs::kFamilyUserMetricsSessionEngagementStartTime);
}

}  // namespace chromeos
