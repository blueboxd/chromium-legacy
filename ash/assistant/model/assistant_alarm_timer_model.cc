// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_alarm_timer_model.h"

#include <utility>

#include "ash/assistant/model/assistant_alarm_timer_model_observer.h"
#include "base/time/time.h"

namespace ash {

AssistantAlarmTimerModel::AssistantAlarmTimerModel() = default;

AssistantAlarmTimerModel::~AssistantAlarmTimerModel() = default;

void AssistantAlarmTimerModel::AddObserver(
    AssistantAlarmTimerModelObserver* observer) const {
  observers_.AddObserver(observer);
}

void AssistantAlarmTimerModel::RemoveObserver(
    AssistantAlarmTimerModelObserver* observer) const {
  observers_.RemoveObserver(observer);
}

void AssistantAlarmTimerModel::AddOrUpdateTimer(AssistantTimerPtr timer) {
  auto* ptr = timer.get();

  auto it = timers_.find(timer->id);
  if (it == timers_.end()) {
    timer->creation_time = timer->creation_time.value_or(base::Time::Now());
    timers_[ptr->id] = std::move(timer);
    NotifyTimerAdded(*ptr);
    return;
  }

  // If not explicitly provided, carry forward |creation_time|. This allows us
  // to track the lifetime of |timer| across updates.
  timer->creation_time =
      timer->creation_time.value_or(timers_[ptr->id]->creation_time.value());

  timers_[ptr->id] = std::move(timer);
  NotifyTimerUpdated(*ptr);
}

void AssistantAlarmTimerModel::RemoveTimer(const std::string& id) {
  auto it = timers_.find(id);
  if (it == timers_.end())
    return;

  AssistantTimerPtr timer = std::move(it->second);
  timers_.erase(it);

  NotifyTimerRemoved(*timer);
}

void AssistantAlarmTimerModel::RemoveAllTimers() {
  while (!timers_.empty())
    RemoveTimer(timers_.begin()->second->id);
}

std::vector<const AssistantTimer*> AssistantAlarmTimerModel::GetAllTimers()
    const {
  std::vector<const AssistantTimer*> timers;
  for (const auto& pair : timers_)
    timers.push_back(pair.second.get());
  return timers;
}

const AssistantTimer* AssistantAlarmTimerModel::GetTimerById(
    const std::string& id) const {
  auto it = timers_.find(id);
  return it != timers_.end() ? it->second.get() : nullptr;
}

void AssistantAlarmTimerModel::NotifyTimerAdded(const AssistantTimer& timer) {
  for (auto& observer : observers_)
    observer.OnTimerAdded(timer);
}

void AssistantAlarmTimerModel::NotifyTimerUpdated(const AssistantTimer& timer) {
  for (auto& observer : observers_)
    observer.OnTimerUpdated(timer);
}

void AssistantAlarmTimerModel::NotifyTimerRemoved(const AssistantTimer& timer) {
  for (auto& observer : observers_)
    observer.OnTimerRemoved(timer);
}

}  // namespace ash
