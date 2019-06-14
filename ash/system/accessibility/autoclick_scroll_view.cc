// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_scroll_view.h"

#include "ash/autoclick/autoclick_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "base/timer/timer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

// The close button for the automatic clicks scroll bubble.
class AutoclickScrollCloseButton : public TopShortcutButton,
                                   public views::ButtonListener {
 public:
  explicit AutoclickScrollCloseButton()
      : TopShortcutButton(this, IDS_ASH_AUTOCLICK_SCROLL_CLOSE) {
    views::View::SetID(
        static_cast<int>(AutoclickScrollView::ButtonId::kCloseScroll));
    EnableCanvasFlippingForRTLUI(false);
    SetPreferredSize(gfx::Size(48, 48));
    // TODO(katie): set size and icon when spec is available.
  }

  ~AutoclickScrollCloseButton() override = default;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    Shell::Get()->autoclick_controller()->DoScrollAction(
        AutoclickController::ScrollPadAction::kScrollClose);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoclickScrollCloseButton);
};

// A single scroll button (up/down/left/right) for automatic clicks scroll
// bubble.
// TODO(katie): Implement custom UI instead of TopShortcutButton.
class AutoclickScrollButton : public TopShortcutButton,
                              public views::ButtonListener {
 public:
  AutoclickScrollButton(AutoclickController::ScrollPadAction action,
                        int accessible_name_id,
                        AutoclickScrollView::ButtonId id)
      : TopShortcutButton(this, accessible_name_id), action_(action) {
    views::View::SetID(static_cast<int>(id));
    // Disable canvas flipping, as scroll left should always be left no matter
    // the language orientation.
    EnableCanvasFlippingForRTLUI(false);
    scroll_hover_timer_ = std::make_unique<base::RetainingOneShotTimer>(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(
            int64_t{AutoclickScrollView::kAutoclickScrollDelayMs}),
        base::BindRepeating(&AutoclickScrollButton::DoScrollAction,
                            base::Unretained(this)));
  }

  ~AutoclickScrollButton() override {
    Shell::Get()->autoclick_controller()->OnExitedScrollButton();
  }

  void ProcessAction(AutoclickController::ScrollPadAction action) {
    Shell::Get()->autoclick_controller()->DoScrollAction(action);
    // TODO(katie): Log UMA for scroll user action.
  }

  void DoScrollAction() {
    ProcessAction(action_);
    // Reset the timer to continue to do the action as long as we are hovering.
    scroll_hover_timer_->Reset();
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    ProcessAction(action_);
  }

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override {
    // For the four scroll buttons, set the button to a hovered/active state
    // when this happens. Also tell something (autoclick controller? autoclick
    // scroll controller?) to start the timer that will cause a lot of scrolls
    // to occur, "clicking" on itself multiple times or otherwise.
    scroll_hover_timer_->Reset();
    Shell::Get()->autoclick_controller()->OnEnteredScrollButton();
  }

  // TODO(katie): Determine if this is reliable enough, or if it might not fire
  // in some cases.
  void OnMouseExited(const ui::MouseEvent& event) override {
    // For the four scroll buttons, unset the hover state when this happens.
    if (scroll_hover_timer_->IsRunning())
      scroll_hover_timer_->Stop();

    // Allow the Autoclick timer and widget to restart.
    Shell::Get()->autoclick_controller()->OnExitedScrollButton();
  }

 private:
  const AutoclickController::ScrollPadAction action_;
  std::unique_ptr<base::RetainingOneShotTimer> scroll_hover_timer_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickScrollButton);
};

// ------ AutoclickScrollBubbleView  ------ //

AutoclickScrollBubbleView::AutoclickScrollBubbleView(
    TrayBubbleView::InitParams init_params)
    : TrayBubbleView(init_params) {}

AutoclickScrollBubbleView::~AutoclickScrollBubbleView() {}

void AutoclickScrollBubbleView::UpdateAnchorRect(const gfx::Rect& rect) {
  ui::ScopedLayerAnimationSettings settings(
      GetWidget()->GetLayer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  SetAnchorRect(rect);
}

bool AutoclickScrollBubbleView::IsAnchoredToStatusArea() const {
  return false;
}

const char* AutoclickScrollBubbleView::GetClassName() const {
  return "AutoclickScrollBubbleView";
}

// ------ AutoclickScrollView  ------ //

AutoclickScrollView::AutoclickScrollView() {
  AutoclickScrollButton* scroll_up_button = new AutoclickScrollButton(
      AutoclickController::ScrollPadAction::kScrollUp,
      IDS_ASH_AUTOCLICK_SCROLL_UP, ButtonId::kScrollUp);

  AutoclickScrollButton* scroll_down_button = new AutoclickScrollButton(
      AutoclickController::ScrollPadAction::kScrollDown,
      IDS_ASH_AUTOCLICK_SCROLL_DOWN, ButtonId::kScrollDown);
  AutoclickScrollButton* scroll_left_button = new AutoclickScrollButton(
      AutoclickController::ScrollPadAction::kScrollLeft,
      IDS_ASH_AUTOCLICK_SCROLL_LEFT, ButtonId::kScrollLeft);
  AutoclickScrollButton* scroll_right_button = new AutoclickScrollButton(
      AutoclickController::ScrollPadAction::kScrollRight,
      IDS_ASH_AUTOCLICK_SCROLL_RIGHT, ButtonId::kScrollRight);
  AutoclickScrollCloseButton* close_scroll_button =
      new AutoclickScrollCloseButton();
  // TODO(katie): Layout to match spec.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(), 0));
  layout->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kEnd);
  AddChildView(scroll_up_button);
  AddChildView(scroll_down_button);
  AddChildView(scroll_left_button);
  AddChildView(scroll_right_button);
  AddChildView(close_scroll_button);
}

}  // namespace ash
