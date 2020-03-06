// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/activity_storage.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"

namespace policy {

// Activity periods are keyed with day and user in format:
// '<day_timestamp>:<BASE64 encoded user email>'
constexpr char kActivityKeySeparator = ':';

ActivityStorage::ActivityStorage(PrefService* pref_service,
                                 const std::string& pref_name,
                                 base::TimeDelta day_start_offset)
    : pref_service_(pref_service),
      pref_name_(pref_name),
      day_start_offset_(day_start_offset) {
  DCHECK(pref_service_);
  const PrefService::PrefInitializationStatus pref_service_status =
      pref_service_->GetInitializationStatus();
  DCHECK(pref_service_status != PrefService::INITIALIZATION_STATUS_WAITING &&
         pref_service_status != PrefService::INITIALIZATION_STATUS_ERROR);
}

ActivityStorage::~ActivityStorage() = default;

base::Time ActivityStorage::GetBeginningOfDay(base::Time timestamp) {
  return timestamp.LocalMidnight() + day_start_offset_;
}

void ActivityStorage::PruneActivityPeriods(
    base::Time base_time,
    base::TimeDelta max_past_activity_interval,
    base::TimeDelta max_future_activity_interval) {
  base::Time min_time = base_time - max_past_activity_interval;
  base::Time max_time = base_time + max_future_activity_interval;
  TrimActivityPeriods(TimestampToDayKey(min_time), TimestampToDayKey(max_time));
}

void ActivityStorage::TrimActivityPeriods(int64_t min_day_key,
                                          int64_t max_day_key) {
  const base::Value* activity_times = pref_service_->GetDictionary(pref_name_);
  // Load collected activities into an interval map.
  auto intervals = GetActivityPeriodsFromPref(*activity_times);

  // Remove data that is too old, or too far in the future.
  intervals.SetInterval(std::numeric_limits<int64_t>::min(), min_day_key,
                        base::nullopt);
  intervals.SetInterval(max_day_key, std::numeric_limits<int64_t>::max(),
                        base::nullopt);

  // Flush the activities into Dictionary
  base::Value copy(base::Value::Type::DICTIONARY);
  for (const auto& interval : intervals) {
    if (!interval.second.has_value()) {
      continue;
    }
    const auto key = MakeActivityPeriodPrefKey(
        interval.first.begin, interval.second.value().user_email);
    copy.SetIntPath(key, interval.first.end - interval.first.begin);
  }

  // Flush the activities into pref_service_
  pref_service_->Set(pref_name_, copy);
}

// static
std::string ActivityStorage::MakeActivityPeriodPrefKey(
    int64_t start,
    const std::string& user_email) {
  const std::string day_key = base::NumberToString(start);
  if (user_email.empty())
    return day_key;

  std::string encoded_email;
  base::Base64Encode(user_email, &encoded_email);
  return day_key + kActivityKeySeparator + encoded_email;
}

// static
bool ActivityStorage::ParseActivityPeriodPrefKey(const std::string& key,
                                                 int64_t* start_timestamp,
                                                 std::string* user_email) {
  auto separator_pos = key.find(kActivityKeySeparator);
  if (separator_pos == std::string::npos) {
    user_email->clear();
    return base::StringToInt64(key, start_timestamp);
  }
  return base::StringToInt64(key.substr(0, separator_pos), start_timestamp) &&
         base::Base64Decode(key.substr(separator_pos + 1), user_email);
}

// static
IntervalMap<int64_t, ActivityStorage::Period>
ActivityStorage::GetActivityPeriodsFromPref(
    const base::Value& stored_activity_periods) {
  IntervalMap<int64_t, Period> activity_periods;
  for (const auto& item : stored_activity_periods.DictItems()) {
    int64_t timestamp;
    ActivePeriod info;
    if (!ParseActivityPeriodPrefKey(item.first, &timestamp, &info.user_email)) {
      LOG(WARNING) << "Cannot parse recorded activity key: '" << item.first
                   << "'";
      continue;
    }
    int duration;
    if (!item.second.GetAsInteger(&duration)) {
      LOG(WARNING) << "Cannot parse recorded activity duration: '"
                   << item.second << "'";
      continue;
    }
    if (duration > 0) {
      activity_periods.SetInterval(timestamp, timestamp + duration, info);
    }
  }
  return activity_periods;
}

int64_t ActivityStorage::TimestampToDayKey(base::Time timestamp) {
  base::Time::Exploded exploded;
  base::Time day_start = GetBeginningOfDay(timestamp);
  // TODO(crbug.com/827386): directly test this time change. Currently it is
  // tested through ScreenTimeControllerBrowsertest.
  if (timestamp < day_start)
    day_start -= base::TimeDelta::FromDays(1);
  day_start.LocalExplode(&exploded);
  base::Time out_time;
  bool conversion_success = base::Time::FromUTCExploded(exploded, &out_time);
  DCHECK(conversion_success);
  return out_time.ToJavaTime();
}

}  // namespace policy
