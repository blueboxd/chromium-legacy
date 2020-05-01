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
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

constexpr int kBackButtonRadiusDip = 18;
constexpr int kBackButtonDiameterDip = 2 * kBackButtonRadiusDip;

// The back button should display above and to the right of the anchor rect
// provided. Because the TrayBubbleView defaults to showing the right edges
// lining up (rather than appearing off to the side) we'll add the width of the
// button to the anchor rect's width.
gfx::Rect AdjustAnchorRect(const gfx::Rect& anchor) {
  gfx::Rect adjusted_anchor(anchor.x(), anchor.y(),
                            anchor.width() + kBackButtonDiameterDip,
                            anchor.height());

  // Don't allow our new rect to extend past the edge of the display.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(anchor);
  adjusted_anchor.Intersect(display.bounds());
  return adjusted_anchor;
}

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
  if (!widget_) {
    TrayBubbleView::InitParams init_params;
    init_params.delegate = this;
    // Anchor within the overlay container.
    init_params.parent_window =
        Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                            kShellWindowId_AccessibilityBubbleContainer);
    init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
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

  DCHECK(bubble_view_);
  bubble_view_->ChangeAnchorRect(AdjustAnchorRect(anchor));
  widget_->Show();
}

void SwitchAccessBackButtonBubbleController::Hide() {
  if (widget_)
    widget_->Hide();
}

void SwitchAccessBackButtonBubbleController::BubbleViewDestroyed() {
  back_button_view_ = nullptr;
  bubble_view_ = nullptr;
  widget_ = nullptr;
}

}  // namespace ash
