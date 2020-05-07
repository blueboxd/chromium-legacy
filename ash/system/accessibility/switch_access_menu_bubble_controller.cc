// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_menu_bubble_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/accessibility/switch_access_back_button_bubble_controller.h"
#include "ash/system/accessibility/switch_access_menu_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"

namespace ash {

SwitchAccessMenuBubbleController::SwitchAccessMenuBubbleController()
    : back_button_controller_(
          std::make_unique<SwitchAccessBackButtonBubbleController>()) {}

SwitchAccessMenuBubbleController::~SwitchAccessMenuBubbleController() {
  if (widget_ && !widget_->IsClosed())
    widget_->CloseNow();
}

void SwitchAccessMenuBubbleController::ShowBackButton(const gfx::Rect& anchor) {
  back_button_controller_->ShowBackButton(anchor, /*showFocusRing=*/true);
}

void SwitchAccessMenuBubbleController::ShowMenu(
    const gfx::Rect& anchor,
    const std::vector<std::string>& actions_to_show) {
  if (!widget_) {
    TrayBubbleView::InitParams init_params;
    init_params.delegate = this;
    // Anchor within the overlay container.
    init_params.parent_window =
        Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                            kShellWindowId_AccessibilityBubbleContainer);
    init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
    init_params.is_anchored_to_status_area = false;
    init_params.insets = gfx::Insets(kUnifiedMenuPadding, kUnifiedMenuPadding);
    init_params.corner_radius = kUnifiedTrayCornerRadius;
    init_params.has_shadow = false;
    init_params.translucent = true;
    bubble_view_ = new TrayBubbleView(init_params);

    menu_view_ = new SwitchAccessMenuView();
    menu_view_->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(kUnifiedMenuPadding)));
    bubble_view_->AddChildView(menu_view_);

    menu_view_->SetPaintToLayer();
    menu_view_->layer()->SetFillsBoundsOpaquely(false);

    widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
    TrayBackgroundView::InitializeBubbleAnimations(widget_);
    bubble_view_->InitializeAndShowBubble();

    CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
        widget_->GetNativeWindow(),
        CollisionDetectionUtils::RelativePriority::kSwitchAccessMenu);
  }

  DCHECK(bubble_view_);

  menu_view_->SetActions(actions_to_show);
  bubble_view_->SetPreferredWidth(menu_view_->GetBubbleWidthDip());
  bubble_view_->ChangeAnchorRect(anchor);

  gfx::Rect new_bounds = widget_->GetWindowBoundsInScreen();

  // Update the preferred bounds based on other system windows.
  gfx::Rect resting_bounds = CollisionDetectionUtils::AvoidObstacles(
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget_->GetNativeWindow()),
      new_bounds, CollisionDetectionUtils::RelativePriority::kSwitchAccessMenu);

  widget_->SetBounds(resting_bounds);
  widget_->Show();
  back_button_controller_->ShowBackButton(resting_bounds,
                                          /*showFocusRing=*/false);
}

void SwitchAccessMenuBubbleController::HideBackButton() {
  if (menu_view_ && menu_view_->IsDrawn())
    back_button_controller_->HideFocusRing();
  else
    back_button_controller_->Hide();
}

void SwitchAccessMenuBubbleController::HideMenuBubble() {
  back_button_controller_->Hide();
  if (widget_)
    widget_->Hide();
}

void SwitchAccessMenuBubbleController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  menu_view_ = nullptr;
  widget_ = nullptr;
}

}  // namespace ash
