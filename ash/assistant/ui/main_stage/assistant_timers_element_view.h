// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TIMERS_ELEMENT_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TIMERS_ELEMENT_VIEW_H_

#include <string>

#include "ash/assistant/ui/base/assistant_scroll_view.h"
#include "ash/assistant/ui/main_stage/assistant_ui_element_view.h"
#include "base/component_export.h"

namespace ash {

class AssistantTimersElement;
class AssistantViewDelegate;

// AssistantTimersElementView is the visual representation of an
// AssistantTimersElement. It is a child view of UiElementContainerView.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantTimersElementView
    : public AssistantUiElementView,
      public AssistantScrollView::Observer {
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
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // AssistantScrollView::Observer:
  void OnContentsPreferredSizeChanged(views::View* content_view) override;

 private:
  void InitLayout(AssistantViewDelegate* delegate,
                  const AssistantTimersElement* timers_element);

  AssistantScrollView* scroll_view_;  // Owned by view hierarchy.
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TIMERS_ELEMENT_VIEW_H_
