// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_accessibility_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/work_area_insets.h"
#include "base/logging.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

namespace {

constexpr int kFloatingMenuHeight = 64;
constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(150);

FloatingMenuPosition DefaultSystemPosition() {
  return base::i18n::IsRTL() ? FloatingMenuPosition::kBottomLeft
                             : FloatingMenuPosition::kBottomRight;
}

}  // namespace

FloatingAccessibilityController::FloatingAccessibilityController() {
  Shell::Get()->locale_update_controller()->AddObserver(this);
}
FloatingAccessibilityController::~FloatingAccessibilityController() {
  Shell::Get()->locale_update_controller()->RemoveObserver(this);
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
}

void FloatingAccessibilityController::Show(FloatingMenuPosition position) {
  // Kiosk check.
  if (!Shell::Get()->session_controller()->IsRunningInAppMode()) {
    NOTREACHED()
        << "Floating accessibility menu can only be run in a kiosk session.";
    return;
  }

  DCHECK(!bubble_view_);

  position_ = position;
  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  // We need our view to be on the same level as the autoclicks menu, so neither
  // of them is overlapping each other.
  init_params.parent_window = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AutoclickContainer);
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  // The widget's shadow is drawn below and on the sides of the view, with a
  // width of kCollisionWindowWorkAreaInsetsDp. Set the top inset to 0 to ensure
  // the scroll view is drawn at kCollisionWindowWorkAreaInsetsDp above the
  // bubble menu when the position is at the bottom of the screen. The space
  // between the bubbles belongs to the scroll view bubble's shadow.
  init_params.insets = gfx::Insets(0, kCollisionWindowWorkAreaInsetsDp,
                                   kCollisionWindowWorkAreaInsetsDp,
                                   kCollisionWindowWorkAreaInsetsDp);
  init_params.max_height = kFloatingMenuHeight;
  init_params.corner_radius = kUnifiedTrayCornerRadius;
  init_params.has_shadow = false;
  init_params.translucent = true;
  bubble_view_ = new FloatingAccessibilityBubbleView(init_params);

  menu_view_ = new FloatingAccessibilityView(this);
  menu_view_->SetBorder(
      views::CreateEmptyBorder(kUnifiedTopShortcutSpacing, 0, 0, 0));
  bubble_view_->AddChildView(menu_view_);

  menu_view_->SetPaintToLayer();
  menu_view_->layer()->SetFillsBoundsOpaquely(false);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  SetMenuPosition(position_);
}

void FloatingAccessibilityController::SetMenuPosition(
    FloatingMenuPosition new_position) {
  if (!menu_view_ || !bubble_view_ || !bubble_widget_)
    return;

  // Update the menu view's UX if the position has changed, or if it's not the
  // default position (because that can change with language direction).
  if (position_ != new_position ||
      new_position == FloatingMenuPosition::kSystemDefault) {
    menu_view_->SetMenuPosition(new_position);
  }
  position_ = new_position;

  // If this is the default system position, pick the position based on the
  // language direction.
  if (new_position == FloatingMenuPosition::kSystemDefault)
    new_position = DefaultSystemPosition();

  // Calculates the ideal bounds.
  // TODO(katie): Support multiple displays: draw the menu on whichever display
  // the cursor is on.
  aura::Window* window = Shell::GetPrimaryRootWindow();
  gfx::Rect work_area =
      WorkAreaInsets::ForWindow(window)->user_work_area_bounds();
  gfx::Rect new_bounds;

  gfx::Size preferred_size = menu_view_->GetPreferredSize();
  switch (new_position) {
    case FloatingMenuPosition::kBottomRight:
      new_bounds = gfx::Rect(work_area.right() - preferred_size.width(),
                             work_area.bottom() - preferred_size.height(),
                             preferred_size.width(), preferred_size.height());
      break;
    case FloatingMenuPosition::kBottomLeft:
      new_bounds =
          gfx::Rect(work_area.x(), work_area.bottom() - preferred_size.height(),
                    preferred_size.width(), preferred_size.height());
      break;
    case FloatingMenuPosition::kTopLeft:
      // Because there is no inset at the top of the widget, add
      // 2 * kCollisionWindowWorkAreaInsetsDp to the top of the work area.
      // to ensure correct padding.
      new_bounds = gfx::Rect(
          work_area.x(), work_area.y() + 2 * kCollisionWindowWorkAreaInsetsDp,
          preferred_size.width(), preferred_size.height());
      break;
    case FloatingMenuPosition::kTopRight:
      // Because there is no inset at the top of the widget, add
      // 2 * kCollisionWindowWorkAreaInsetsDp to the top of the work area.
      // to ensure correct padding.
      new_bounds =
          gfx::Rect(work_area.right() - preferred_size.width(),
                    work_area.y() + 2 * kCollisionWindowWorkAreaInsetsDp,
                    preferred_size.width(), preferred_size.height());
      break;
    case FloatingMenuPosition::kSystemDefault:
      return;
  }

  gfx::Rect resting_bounds =
      CollisionDetectionUtils::AdjustToFitMovementAreaByGravity(
          display::Screen::GetScreen()->GetDisplayNearestWindow(
              bubble_widget_->GetNativeWindow()),
          new_bounds);
  // Un-inset the bounds to get the widget's bounds, which includes the drop
  // shadow.
  resting_bounds.Inset(-kCollisionWindowWorkAreaInsetsDp, 0,
                       -kCollisionWindowWorkAreaInsetsDp,
                       -kCollisionWindowWorkAreaInsetsDp);
  if (bubble_widget_->GetWindowBoundsInScreen() == resting_bounds)
    return;

  ui::ScopedLayerAnimationSettings settings(
      bubble_widget_->GetLayer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTransitionDuration(kAnimationDuration);
  settings.SetTweenType(gfx::Tween::EASE_OUT);
  bubble_widget_->SetBounds(resting_bounds);
}

void FloatingAccessibilityController::OnDetailedMenuEnabled(bool enabled) {
  // TODO(crbug.com/1061068): Implement detailed menu view logic.
  detailed_view_shown_ = enabled;
}

void FloatingAccessibilityController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
  menu_view_ = nullptr;
}

void FloatingAccessibilityController::OnLocaleChanged() {
  // Layout update is needed when language changes between LTR and RTL, if the
  // position is the system default.
  if (position_ == FloatingMenuPosition::kSystemDefault)
    SetMenuPosition(position_);
}

}  // namespace ash
