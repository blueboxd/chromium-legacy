// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_ALARM_TIMER_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/time/time.h"

namespace ash {

// Represents the current state of an Assistant timer.
enum class AssistantTimerState {
  kUnknown,

  // The timer is scheduled to fire at some future date.
  kScheduled,

  // The timer will not fire but is kept in the queue of scheduled events;
  // it can be resumed after which it will fire in |remaining_time|.
  kPaused,

  // The timer has fired. In the simplest case this means the timer has
  // begun ringing.
  kFired,
};

// Models an Assistant timer.
struct AssistantTimer {
  std::string id;
  std::string label;
  AssistantTimerState state{AssistantTimerState::kUnknown};
  base::Time fire_time;
  base::TimeDelta remaining_time;
};
using AssistantTimerPtr = std::unique_ptr<AssistantTimer>;

// Interface to the AssistantAlarmTimerController which is owned by the
// AssistantController. Currently used by the Assistant service to notify Ash
// of changes to the underlying alarm/timer state in LibAssistant.
class ASH_PUBLIC_EXPORT AssistantAlarmTimerController {
 public:
  AssistantAlarmTimerController();
  AssistantAlarmTimerController(const AssistantAlarmTimerController&) = delete;
  AssistantAlarmTimerController& operator=(
      const AssistantAlarmTimerController&) = delete;
  virtual ~AssistantAlarmTimerController();

  static AssistantAlarmTimerController* Get();

  // Invoked when timer state has changed. Note that |timers| may be empty.
  virtual void OnTimerStateChanged(std::vector<AssistantTimerPtr> timers) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
