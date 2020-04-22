// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_alarm_timer_model.h"
#include "ash/assistant/model/assistant_alarm_timer_model_observer.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/mojom/assistant_controller.mojom.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

namespace assistant {
namespace util {
enum class AlarmTimerAction;
}  // namespace util
}  // namespace assistant

class AssistantControllerImpl;

// The AssistantAlarmTimerController is a sub-controller of AssistantController
// tasked with tracking alarm/timer state and providing alarm/timer APIs.
class AssistantAlarmTimerController
    : public mojom::AssistantAlarmTimerController,
      public AssistantControllerObserver,
      public AssistantStateObserver,
      public AssistantAlarmTimerModelObserver {
 public:
  explicit AssistantAlarmTimerController(
      AssistantControllerImpl* assistant_controller);
  ~AssistantAlarmTimerController() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::AssistantAlarmTimerController> receiver);

  // Returns the underlying model.
  const AssistantAlarmTimerModel* model() const { return &model_; }

  // Adds/removes the specified model |observer|.
  void AddModelObserver(AssistantAlarmTimerModelObserver* observer);
  void RemoveModelObserver(AssistantAlarmTimerModelObserver* observer);

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // AssistantStateObserver:
  void OnAssistantStatusChanged(mojom::AssistantState state) override;

  // mojom::AssistantAlarmTimerController:
  void OnTimerStateChanged(
      std::vector<mojom::AssistantTimerPtr> timers) override;

  // AssistantAlarmTimerModelObserver:
  void OnTimerAdded(const mojom::AssistantTimer& timer) override;
  void OnTimerUpdated(const mojom::AssistantTimer& timer) override;
  void OnTimerRemoved(const mojom::AssistantTimer& timer) override;
  void OnAllTimersRemoved() override;

 private:
  void PerformAlarmTimerAction(const assistant::util::AlarmTimerAction& action,
                               const std::string& alarm_timer_id,
                               const base::Optional<base::TimeDelta>& duration);

  AssistantControllerImpl* const assistant_controller_;  // Owned by Shell.

  mojo::Receiver<mojom::AssistantAlarmTimerController> receiver_{this};

  AssistantAlarmTimerModel model_;

  base::RepeatingTimer ticker_;

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_;

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantAlarmTimerController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
