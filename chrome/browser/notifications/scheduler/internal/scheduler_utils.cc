// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"

#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"

namespace notifications {

bool ToLocalHour(int hour,
                 const base::Time& today,
                 int day_delta,
                 base::Time* out) {
  DCHECK_GE(hour, 0);
  DCHECK_LE(hour, 23);
  DCHECK(out);

  // Gets the local time at |hour| in yesterday.
  base::Time another_day = today + base::TimeDelta::FromDays(day_delta);
  base::Time::Exploded another_day_exploded;
  another_day.LocalExplode(&another_day_exploded);
  another_day_exploded.hour = hour;
  another_day_exploded.minute = 0;
  another_day_exploded.second = 0;
  another_day_exploded.millisecond = 0;

  // Converts local exploded time to time stamp.
  return base::Time::FromLocalExploded(another_day_exploded, out);
}

void NotificationsShownToday(
    const std::map<SchedulerClientType, const ClientState*>& client_states,
    std::map<SchedulerClientType, int>* shown_per_type,
    int* shown_total,
    SchedulerClientType* last_shown_type) {
  base::Time last_shown_time;
  base::Time now(base::Time::Now());
  base::Time beginning_of_today;
  bool success = ToLocalHour(0, now, 0, &beginning_of_today);
  DCHECK(success);

  for (const auto& state : client_states) {
    const auto* client_state = state.second;
    for (const auto& impression : client_state->impressions) {
      // Tracks last notification shown to the user.
      if (impression.create_time > last_shown_time) {
        last_shown_time = impression.create_time;
        *last_shown_type = client_state->type;
      }

      // Count notification shown today.
      if (impression.create_time >= beginning_of_today) {
        (*shown_per_type)[client_state->type]++;
        ++(*shown_total);
      }
    }
  }
}

int NotificationsShownToday(ClientState* state) {
  int count = 0;
  auto impressions = state->impressions;
  base::Time now(base::Time::Now());
  base::Time beginning_of_today, beginning_of_tomorrow;
  bool success = ToLocalHour(0, now, 0, &beginning_of_today);
  beginning_of_tomorrow += base::TimeDelta::FromDays(1);
  DCHECK(success);
  if (impressions.rbegin()->create_time < beginning_of_today) {
    count = 0;
  } else if (impressions.begin()->create_time > beginning_of_tomorrow) {
    count = impressions.size();
  } else {
    auto right = lower_bound(impressions.rbegin(), impressions.rend(),
                             beginning_of_today,
                             [](const Impression& lhs, const base::Time& rhs) {
                               return lhs.create_time < rhs;
                             });
    auto left = upper_bound(impressions.rbegin(), impressions.rend(),
                            beginning_of_tomorrow,
                            [](const base::Time& lhs, const Impression& rhs) {
                              return lhs < rhs.create_time;
                            });
    count = right - left + 1;
  }
  DCHECK_LE(count, state->current_max_daily_show);
  return count;
}

std::unique_ptr<ClientState> CreateNewClientState(
    SchedulerClientType type,
    const SchedulerConfig& config) {
  auto client_state = std::make_unique<ClientState>();
  client_state->type = type;
  client_state->current_max_daily_show = config.initial_daily_shown_per_type;
  return client_state;
}

}  // namespace notifications
