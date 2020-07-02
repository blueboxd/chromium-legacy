// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_ALARM_TIMER_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/optional.h"
#include "base/time/time.h"

namespace ash {

class AssistantAlarmTimerModel;

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
struct ASH_PUBLIC_EXPORT AssistantTimer {
  AssistantTimer();
  AssistantTimer(const AssistantTimer&);
  AssistantTimer& operator=(const AssistantTimer&);
  ~AssistantTimer();

  // Returns whether |this| is considered equal in LibAssistant to |other|.
  // NOTE: this *only* checks against fields which are set by LibAssistant.
  bool IsEqualInLibAssistantTo(const AssistantTimer& other) const;

  // These fields are set *only* by LibAssistant.
  std::string id;
  std::string label;
  base::Time fire_time;
  base::TimeDelta original_duration;

  // These fields are set *only* by Chrome.
  base::Optional<base::Time> creation_time;
  base::TimeDelta remaining_time;

  // This field is set *only* by LibAssistant *except* in timers v2 where we may
  // update a timer from |kScheduled| to |kFired| state in Chrome in order to
  // reduce UI jank that would otherwise occur if we waited for LibAssistant to
  // notify us of the state change.
  AssistantTimerState state{AssistantTimerState::kUnknown};
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

  // Returns the singleton instance owned by AssistantController.
  static AssistantAlarmTimerController* Get();

  // Returns a pointer to the underlying model.
  virtual const AssistantAlarmTimerModel* GetModel() const = 0;

  // Invoked when timer state has changed. Note that |timers| may be empty.
  virtual void OnTimerStateChanged(std::vector<AssistantTimerPtr> timers) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
