// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "ui/gfx/codec/png_codec.h"

namespace notifications {
namespace {
using FirstAndLastIters = std::pair<std::deque<Impression>::const_iterator,
                                    std::deque<Impression>::const_iterator>;

base::Optional<FirstAndLastIters> FindFirstAndLastNotificationShownToday(
    const std::deque<Impression>& impressions,
    const base::Time& now,
    const base::Time& beginning_of_today) {
  if (impressions.empty() || impressions.cbegin()->create_time > now ||
      impressions.crbegin()->create_time < beginning_of_today)
    return base::nullopt;

  auto last =
      std::upper_bound(impressions.cbegin(), impressions.cend(), now,
                       [](const base::Time& lhs, const Impression& rhs) {
                         return lhs < rhs.create_time;
                       });

  auto first =
      std::lower_bound(impressions.cbegin(), last, beginning_of_today,
                       [](const Impression& lhs, const base::Time& rhs) {
                         return lhs.create_time < rhs;
                       });
  return base::make_optional<FirstAndLastIters>(first, last - 1);
}

// Converts SkBitmap to String.
std::string ConvertIconToStringOnIOThread(SkBitmap image) {
  base::AssertLongCPUWorkAllowed();
  std::vector<unsigned char> image_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(std::move(image), false, &image_data);
  std::string result(image_data.begin(), image_data.end());
  return result;
}

// Converts SkBitmap to String.
SkBitmap ConvertStringToIconOnIOThread(std::string data) {
  SkBitmap image;
  gfx::PNGCodec::Decode(reinterpret_cast<const unsigned char*>(data.data()),
                        data.length(), &image);
  return image;
}
}  // namespace

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

int NotificationsShownToday(const ClientState* state, base::Clock* clock) {
  std::map<SchedulerClientType, const ClientState*> client_states;
  std::map<SchedulerClientType, int> shown_per_type;
  client_states.emplace(state->type, state);
  int shown_total = 0;
  SchedulerClientType last_shown_type = SchedulerClientType::kUnknown;
  NotificationsShownToday(client_states, &shown_per_type, &shown_total,
                          &last_shown_type, clock);
  return shown_per_type[state->type];
}

void NotificationsShownToday(
    const std::map<SchedulerClientType, const ClientState*>& client_states,
    std::map<SchedulerClientType, int>* shown_per_type,
    int* shown_total,
    SchedulerClientType* last_shown_type,
    base::Clock* clock) {
  base::Time last_shown_time;
  base::Time now = clock->Now();
  base::Time beginning_of_today;
  bool success = ToLocalHour(0, now, 0, &beginning_of_today);
  DCHECK(success);
  for (const auto& state : client_states) {
    auto* client_state = state.second;

    auto iter_pair = FindFirstAndLastNotificationShownToday(
        client_state->impressions, now, beginning_of_today);

    if (!iter_pair)
      continue;

    if (iter_pair->second != client_state->impressions.cend())
      DLOG(ERROR) << "Wrong format: time stamped to the future! "
                  << iter_pair->second->create_time;

    if (iter_pair->second->create_time > last_shown_time) {
      last_shown_time = iter_pair->second->create_time;
      *last_shown_type = client_state->type;
    }

    int count = std::distance(iter_pair->first, iter_pair->second) + 1;
    (*shown_per_type)[client_state->type] = count;
    (*shown_total) += count;
  }
}

std::unique_ptr<ClientState> CreateNewClientState(
    SchedulerClientType type,
    const SchedulerConfig& config) {
  auto client_state = std::make_unique<ClientState>();
  client_state->type = type;
  client_state->current_max_daily_show = config.initial_daily_shown_per_type;
  return client_state;
}

// Converts SkBitmap to String.
void ConvertIconToString(SkBitmap image,
                         base::OnceCallback<void(std::string)> callback) {
  DCHECK(callback);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ConvertIconToStringOnIOThread, std::move(image)),
      std::move(callback));
}

// Converts String to SkBitmap.
void ConvertStringToIcon(std::string data,
                         base::OnceCallback<void(SkBitmap)> callback) {
  DCHECK(callback);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ConvertStringToIconOnIOThread, std::move(data)),
      std::move(callback));
}

}  // namespace notifications
