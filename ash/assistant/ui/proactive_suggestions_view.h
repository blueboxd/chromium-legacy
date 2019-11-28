// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_VIEW_H_
#define ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_VIEW_H_

#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class AssistantViewDelegate;
class ProactiveSuggestions;

// Base entry point for the proactive suggestions feature.
class COMPONENT_EXPORT(ASSISTANT_UI) ProactiveSuggestionsView
    : public views::Button,
      public views::ButtonListener,
      public views::WidgetObserver,
      public aura::WindowObserver,
      public display::DisplayObserver,
      public KeyboardControllerObserver {
 public:
  ~ProactiveSuggestionsView() override;

  // Invoked to initialize the view. This method should be called immediately
  // following construction to ready the view for display.
  void Init();

  // views::Button:
  const char* GetClassName() const override;
  void PreferredSizeChanged() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {}

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // KeyboardControllerObserver:
  void OnKeyboardOccludedBoundsChanged(
      const gfx::Rect& new_bounds_in_screen) override;

  // Shows/hides/closes the view.
  // Note that ShowWhenReady() may show the widget for the view asynchronously
  // if it is unsafe to show immediately without possibly causing UI jank.
  virtual void ShowWhenReady();
  virtual void Hide();
  virtual void Close();

  const ProactiveSuggestions* proactive_suggestions() const {
    return proactive_suggestions_.get();
  }

 protected:
  explicit ProactiveSuggestionsView(AssistantViewDelegate* delegate);

  // Invoked during the Init() sequence to initialize layout.
  virtual void InitLayout() {}

  AssistantViewDelegate* delegate() { return delegate_; }

 private:
  void InitWidget();
  void InitWindow();
  void UpdateBounds();

  AssistantViewDelegate* const delegate_;

  // The set of proactive suggestions associated with this view.
  scoped_refptr<const ProactiveSuggestions> proactive_suggestions_;

  // Cached bounds of workspace occluded by the keyboard for determining usable
  // work area in which to lay out this view's widget.
  gfx::Rect keyboard_workspace_occluded_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ProactiveSuggestionsView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_PROACTIVE_SUGGESTIONS_VIEW_H_
