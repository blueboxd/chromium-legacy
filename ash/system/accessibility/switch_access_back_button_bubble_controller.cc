// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_back_button_bubble_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/accessibility/switch_access_back_button_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_view.h"

namespace ash {

namespace {
constexpr int kBackButtonRadiusDip = 18;
constexpr int kBackButtonDiameterDip = 2 * kBackButtonRadiusDip;
}  // namespace

SwitchAccessBackButtonBubbleController::
    SwitchAccessBackButtonBubbleController() {}

SwitchAccessBackButtonBubbleController::
    ~SwitchAccessBackButtonBubbleController() {
  if (widget_ && !widget_->IsClosed())
    widget_->CloseNow();
}

void SwitchAccessBackButtonBubbleController::ShowBackButton(
    const gfx::Rect& anchor) {
  if (widget_) {
    DCHECK(bubble_view_);
    bubble_view_->ChangeAnchorRect(anchor);
    return;
  }

  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  // Anchor within the overlay container.
  init_params.parent_window =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_AccessibilityPanelContainer);
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = anchor;
  init_params.is_anchored_to_status_area = false;
  init_params.has_shadow = false;

  // The back button is a circle, so the preferred width and height are the
  // diameter, and the corner radius is the circle radius.
  init_params.corner_radius = kBackButtonRadiusDip;
  init_params.preferred_width = kBackButtonDiameterDip;
  init_params.max_height = kBackButtonDiameterDip;

  bubble_view_ = new TrayBubbleView(init_params);

  back_button_view_ = new SwitchAccessBackButtonView(kBackButtonDiameterDip);
  back_button_view_->SetBackground(UnifiedSystemTrayView::CreateBackground());
  bubble_view_->AddChildView(back_button_view_);
  bubble_view_->set_color(SK_ColorTRANSPARENT);
  bubble_view_->layer()->SetFillsBoundsOpaquely(false);

  widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  TrayBackgroundView::InitializeBubbleAnimations(widget_);
  bubble_view_->InitializeAndShowBubble();
}

void SwitchAccessBackButtonBubbleController::CloseBubble() {
  if (widget_ && !widget_->IsClosed())
    widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void SwitchAccessBackButtonBubbleController::BubbleViewDestroyed() {
  back_button_view_ = nullptr;
  bubble_view_ = nullptr;
  widget_ = nullptr;
}

}  // namespace ash
