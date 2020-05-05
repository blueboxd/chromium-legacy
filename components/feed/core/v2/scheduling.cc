// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/scheduling.h"

#include "base/time/time.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "components/feed/core/v2/config.h"

namespace feed {
namespace {

base::Value VectorToValue(const std::vector<base::TimeDelta>& values) {
  base::Value result(base::Value::Type::LIST);
  for (base::TimeDelta delta : values) {
    result.Append(CreateTimeDeltaValue(delta));
  }
  return result;
}

bool ValueToVector(const base::Value& value,
                   std::vector<base::TimeDelta>* result) {
  if (!value.is_list())
    return false;
  for (const base::Value& entry : value.GetList()) {
    base::TimeDelta delta;
    if (!GetValueAsTimeDelta(entry, &delta))
      return false;
    result->push_back(delta);
  }
  return true;
}
}  // namespace

RequestSchedule::RequestSchedule() = default;
RequestSchedule::~RequestSchedule() = default;
RequestSchedule::RequestSchedule(const RequestSchedule&) = default;
RequestSchedule& RequestSchedule::operator=(const RequestSchedule&) = default;

base::Value RequestScheduleToValue(const RequestSchedule& schedule) {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetKey("anchor", CreateTimeValue(schedule.anchor_time));
  result.SetKey("offsets", VectorToValue(schedule.refresh_offsets));
  return result;
}

RequestSchedule RequestScheduleFromValue(const base::Value& value) {
  if (!value.is_dict())
    return {};
  RequestSchedule result;
  const base::Value* anchor = value.FindKey("anchor");
  const base::Value* offsets =
      value.FindKeyOfType("offsets", base::Value::Type::LIST);
  if (!anchor || !offsets)
    return {};
  if (!GetValueAsTime(*anchor, &result.anchor_time) ||
      !ValueToVector(*offsets, &result.refresh_offsets))
    return {};
  return result;
}

base::Time NextScheduledRequestTime(base::Time now, RequestSchedule* schedule) {
  if (schedule->refresh_offsets.empty())
    return now + GetFeedConfig().default_background_refresh_interval;
  // Attempt to detect system clock changes. If |anchor_time| is in the future,
  // or too far in the past, we reset |anchor_time| to now.
  if (now < schedule->anchor_time ||
      schedule->anchor_time + base::TimeDelta::FromDays(7) < now) {
    schedule->anchor_time = now;
  }
  while (!schedule->refresh_offsets.empty()) {
    base::Time request_time =
        schedule->anchor_time + schedule->refresh_offsets[0];
    schedule->refresh_offsets.erase(schedule->refresh_offsets.begin());
    if (request_time < now) {
      // The schedule time is in the past. This is most likely to happen if we
      // fail to run one of our scheduled fetches. Just ignore this fetch so
      // that we don't risk multiple fetches at a time.
      continue;
    }
    return request_time;
  }
  return now + GetFeedConfig().default_background_refresh_interval;
}

bool ShouldWaitForNewContent(bool has_content, base::TimeDelta content_age) {
  return !has_content || content_age > GetFeedConfig().stale_content_threshold;
}

}  // namespace feed
