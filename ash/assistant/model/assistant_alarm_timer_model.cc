// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_alarm_timer_model.h"

#include "ash/assistant/model/assistant_alarm_timer_model_observer.h"
#include "ash/public/mojom/assistant_controller.mojom.h"
#include "base/time/time.h"

namespace ash {

AssistantAlarmTimerModel::AssistantAlarmTimerModel() = default;

AssistantAlarmTimerModel::~AssistantAlarmTimerModel() = default;

void AssistantAlarmTimerModel::AddObserver(
    AssistantAlarmTimerModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantAlarmTimerModel::RemoveObserver(
    AssistantAlarmTimerModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantAlarmTimerModel::AddOrUpdateTimer(
    mojom::AssistantTimerPtr timer) {
  auto* ptr = timer.get();

  auto it = timers_.find(timer->id);
  if (it == timers_.end()) {
    timers_[ptr->id] = std::move(timer);
    NotifyTimerAdded(*ptr);
    return;
  }

  timers_[ptr->id] = std::move(timer);
  NotifyTimerUpdated(*ptr);
}

void AssistantAlarmTimerModel::RemoveTimer(const std::string& id) {
  auto it = timers_.find(id);
  if (it == timers_.end())
    return;

  mojom::AssistantTimerPtr timer = std::move(it->second);
  timers_.erase(it);

  NotifyTimerRemoved(*timer);
}

void AssistantAlarmTimerModel::RemoveAllTimers() {
  if (timers_.empty())
    return;

  timers_.clear();
  NotifyAllTimersRemoved();
}

std::vector<const mojom::AssistantTimer*>
AssistantAlarmTimerModel::GetAllTimers() const {
  std::vector<const mojom::AssistantTimer*> timers;
  for (const auto& pair : timers_)
    timers.push_back(pair.second.get());
  return timers;
}

const mojom::AssistantTimer* AssistantAlarmTimerModel::GetTimerById(
    const std::string& id) const {
  auto it = timers_.find(id);
  return it != timers_.end() ? it->second.get() : nullptr;
}

void AssistantAlarmTimerModel::Tick() {
  if (timers_.empty())
    return;

  for (auto& pair : timers_) {
    mojom::AssistantTimer* timer = pair.second.get();
    if (timer->state == mojom::AssistantTimerState::kPaused)
      continue;

    timer->remaining_time = timer->fire_time - base::Time::Now();
    NotifyTimerUpdated(*timer);
  }
}

void AssistantAlarmTimerModel::NotifyTimerAdded(
    const mojom::AssistantTimer& timer) {
  for (auto& observer : observers_)
    observer.OnTimerAdded(timer);
}

void AssistantAlarmTimerModel::NotifyTimerUpdated(
    const mojom::AssistantTimer& timer) {
  for (auto& observer : observers_)
    observer.OnTimerUpdated(timer);
}

void AssistantAlarmTimerModel::NotifyTimerRemoved(
    const mojom::AssistantTimer& timer) {
  for (auto& observer : observers_)
    observer.OnTimerRemoved(timer);
}

void AssistantAlarmTimerModel::NotifyAllTimersRemoved() {
  for (auto& observer : observers_)
    observer.OnAllTimersRemoved();
}

}  // namespace ash
