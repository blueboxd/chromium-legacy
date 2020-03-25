// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TIMERS_ELEMENT_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TIMERS_ELEMENT_VIEW_H_

#include <string>

#include "ash/assistant/model/assistant_alarm_timer_model_observer.h"
#include "ash/assistant/ui/main_stage/assistant_ui_element_view.h"
#include "base/component_export.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AssistantTimersElement;
class AssistantViewDelegate;

// AssistantTimersElementView is the visual representation of an
// AssistantTimersElement. It is a child view of UiElementContainerView.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantTimersElementView
    : public AssistantUiElementView,
      public AssistantAlarmTimerModelObserver {
 public:
  AssistantTimersElementView(AssistantViewDelegate* delegate,
                             const AssistantTimersElement* timers_element);
  AssistantTimersElementView(const AssistantTimersElementView&) = delete;
  AssistantTimersElementView& operator=(const AssistantTimersElementView&) =
      delete;
  ~AssistantTimersElementView() override;

  // AssistantUiElementView:
  const char* GetClassName() const override;
  ui::Layer* GetLayerForAnimating() override;
  std::string ToStringForTesting() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // AssistantAlarmTimerModelObserver:
  void OnTimerUpdated(const mojom::AssistantTimer& timer) override;

 private:
  void InitLayout();
  void UpdateLayout();

  AssistantViewDelegate* const delegate_;  // Owned (indirectly) by Shell.
  views::Label* label_;                    // Owned by view hierarchy.

  const AssistantTimersElement* timers_element_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TIMERS_ELEMENT_VIEW_H_
