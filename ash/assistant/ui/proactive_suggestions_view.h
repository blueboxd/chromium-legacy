// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_VIEW_H_
#define ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_VIEW_H_

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageButton;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

// The view for proactive suggestions which serves as the feature's entry point.
class COMPONENT_EXPORT(ASSISTANT_UI) ProactiveSuggestionsView
    : public views::Button,
      public views::ButtonListener,
      public AssistantUiModelObserver,
      public aura::WindowObserver {
 public:
  explicit ProactiveSuggestionsView(AssistantViewDelegate* delegate);
  ~ProactiveSuggestionsView() override;

  // views::Button:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // AssistantUiModelObserver:
  void OnUsableWorkAreaChanged(const gfx::Rect& usable_work_area) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;

 private:
  void InitLayout();
  void InitWidget();
  void InitWindow();
  void UpdateBounds();

  AssistantViewDelegate* const delegate_;

  views::ImageButton* close_button_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(ProactiveSuggestionsView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_VIEW_H_
