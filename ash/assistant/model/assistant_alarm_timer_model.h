// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_

#include <map>
#include <string>
#include <vector>

#include "ash/public/mojom/assistant_controller.mojom-forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class AssistantAlarmTimerModelObserver;

// The model belonging to AssistantAlarmTimerController which tracks alarm/timer
// state and notifies a pool of observers.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantAlarmTimerModel {
 public:
  AssistantAlarmTimerModel();
  ~AssistantAlarmTimerModel();

  // Adds/removes the specified alarm/timer model |observer|.
  void AddObserver(AssistantAlarmTimerModelObserver* observer);
  void RemoveObserver(AssistantAlarmTimerModelObserver* observer);

  // Adds or updates the timer specified by |timer.id| in the model.
  void AddOrUpdateTimer(mojom::AssistantTimerPtr timer);

  // Removes the timer uniquely identified by |id|.
  void RemoveTimer(const std::string& id);

  // Remove all timers from the model.
  void RemoveAllTimers();

  // Returns all timers from the model.
  std::vector<const mojom::AssistantTimer*> GetAllTimers() const;

  // Returns the timer uniquely identified by |id|.
  const mojom::AssistantTimer* GetTimerById(const std::string& id) const;

  // Invoke to tick any timers. Note that this will update the |remaining_time|
  // for all timers in the model and trigger an OnTimerUpdated() event.
  void Tick();

  // Returns |true| if the model contains no timers, |false| otherwise.
  bool empty() const { return timers_.empty(); }

 private:
  void NotifyTimerAdded(const mojom::AssistantTimer& timer);
  void NotifyTimerUpdated(const mojom::AssistantTimer& timer);
  void NotifyTimerRemoved(const mojom::AssistantTimer& timer);
  void NotifyAllTimersRemoved();

  std::map<std::string, mojom::AssistantTimerPtr> timers_;

  base::ObserverList<AssistantAlarmTimerModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAlarmTimerModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_
