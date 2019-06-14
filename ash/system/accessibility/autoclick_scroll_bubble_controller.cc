// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_scroll_bubble_controller.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_utils.h"

namespace ash {

namespace {
// Autoclick scroll menu constants.
int kAutoclickScrollMenuSizeDips = 250;
}  // namespace

AutoclickScrollBubbleController::AutoclickScrollBubbleController() {}

AutoclickScrollBubbleController::~AutoclickScrollBubbleController() {
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
}

void AutoclickScrollBubbleController::UpdateAnchorRect(
    gfx::Rect rect,
    views::BubbleBorder::Arrow alignment) {
  bubble_view_->SetArrow(alignment);
  bubble_view_->UpdateAnchorRect(rect);
}

void AutoclickScrollBubbleController::ShowBubble(
    gfx::Rect anchor_rect,
    views::BubbleBorder::Arrow alignment) {
  // Ignore if bubble widget already exists.
  if (bubble_widget_)
    return;

  DCHECK(!bubble_view_);

  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  // Anchor within the overlay container.
  init_params.parent_window = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AutoclickContainer);
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = anchor_rect;
  init_params.insets = gfx::Insets(kUnifiedMenuPadding, kUnifiedMenuPadding,
                                   kUnifiedMenuPadding, kUnifiedMenuPadding);
  init_params.min_width = kAutoclickScrollMenuSizeDips;
  init_params.max_width = kAutoclickScrollMenuSizeDips;
  init_params.max_height = kAutoclickScrollMenuSizeDips;
  init_params.corner_radius = kUnifiedTrayCornerRadius;
  init_params.has_shadow = false;
  bubble_view_ = new AutoclickScrollBubbleView(init_params);
  bubble_view_->SetArrow(alignment);

  scroll_view_ = new AutoclickScrollView();
  scroll_view_->SetBackground(UnifiedSystemTrayView::CreateBackground());
  scroll_view_->SetBorder(
      views::CreateEmptyBorder(kUnifiedTopShortcutSpacing, 0, 0, 0));
  bubble_view_->AddChildView(scroll_view_);
  bubble_view_->set_color(SK_ColorTRANSPARENT);
  bubble_view_->layer()->SetFillsBoundsOpaquely(false);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
      bubble_widget_->GetNativeWindow(),
      CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu);
  bubble_view_->InitializeAndShowBubble();

  if (app_list_features::IsBackgroundBlurEnabled()) {
    bubble_widget_->client_view()->layer()->SetBackgroundBlur(
        kUnifiedMenuBackgroundBlur);
  }
}

void AutoclickScrollBubbleController::CloseBubble() {
  if (!bubble_widget_ || bubble_widget_->IsClosed())
    return;
  bubble_widget_->Close();
}

void AutoclickScrollBubbleController::SetBubbleVisibility(bool is_visible) {
  if (!bubble_widget_)
    return;

  if (is_visible)
    bubble_widget_->Show();
  else
    bubble_widget_->Hide();
}

void AutoclickScrollBubbleController::ClickOnBubble(gfx::Point location_in_dips,
                                                    int mouse_event_flags) {
  if (!bubble_widget_)
    return;

  // Change the event location bounds to be relative to the menu bubble.
  location_in_dips -= bubble_view_->GetBoundsInScreen().OffsetFromOrigin();

  // Generate synthesized mouse events for the click.
  const ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, location_in_dips,
                                   location_in_dips, ui::EventTimeForNow(),
                                   mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON);
  const ui::MouseEvent release_event(
      ui::ET_MOUSE_RELEASED, location_in_dips, location_in_dips,
      ui::EventTimeForNow(), mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);

  // Send the press/release events to the widget's root view for processing.
  bubble_widget_->GetRootView()->OnMousePressed(press_event);
  bubble_widget_->GetRootView()->OnMouseReleased(release_event);
}

bool AutoclickScrollBubbleController::ContainsPointInScreen(
    const gfx::Point& point) {
  return bubble_view_ && bubble_view_->GetBoundsInScreen().Contains(point);
}

void AutoclickScrollBubbleController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
  scroll_view_ = nullptr;
}

}  // namespace ash
