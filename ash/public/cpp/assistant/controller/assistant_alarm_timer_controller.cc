// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/controller/assistant_alarm_timer_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

AssistantAlarmTimerController* g_instance = nullptr;

}  // namespace

// AssistantTimer --------------------------------------------------------------

AssistantTimer::AssistantTimer() = default;
AssistantTimer::AssistantTimer(const AssistantTimer&) = default;
AssistantTimer& AssistantTimer::operator=(const AssistantTimer&) = default;
AssistantTimer::~AssistantTimer() = default;

bool AssistantTimer::IsEqualInLibAssistantTo(
    const AssistantTimer& other) const {
  return id == other.id && label == other.label &&
         fire_time == other.fire_time &&
         original_duration == other.original_duration && state == other.state;
}

// AssistantAlarmTimerController -----------------------------------------------

AssistantAlarmTimerController::AssistantAlarmTimerController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AssistantAlarmTimerController::~AssistantAlarmTimerController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AssistantAlarmTimerController* AssistantAlarmTimerController::Get() {
  return g_instance;
}

}  // namespace ash
