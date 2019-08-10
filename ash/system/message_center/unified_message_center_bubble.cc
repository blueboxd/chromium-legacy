// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_center_bubble.h"

#include <memory>

#include "ash/shelf/shelf.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ui/views/widget/widget.h"

namespace ash {

UnifiedMessageCenterBubble::UnifiedMessageCenterBubble(UnifiedSystemTray* tray)
    : tray_(tray) {
  TrayBubbleView::InitParams init_params;
  init_params.delegate = tray;
  // Anchor within the overlay container.
  init_params.parent_window = tray->GetBubbleWindowContainer();
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.min_width = kTrayMenuWidth;
  init_params.max_width = kTrayMenuWidth;
  init_params.corner_radius = kUnifiedTrayCornerRadius;
  init_params.has_shadow = false;

  bubble_view_ = new TrayBubbleView(init_params);

  message_center_view_ =
      bubble_view_->AddChildView(std::make_unique<UnifiedMessageCenterView>(
          nullptr /* parent */, tray->model()));

  message_center_view_->AddObserver(this);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);

  bubble_view_->InitializeAndShowBubble();

  tray->tray_event_filter()->AddBubble(this);

  UpdatePosition();

  if (!message_center_view_->GetVisible()) {
    bubble_widget_->Hide();
  }
}

UnifiedMessageCenterBubble::~UnifiedMessageCenterBubble() {
  tray_->tray_event_filter()->RemoveBubble(this);

  if (bubble_widget_) {
    CHECK(message_center_view_);
    message_center_view_->RemoveObserver(this);

    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }
}

void UnifiedMessageCenterBubble::UpdatePosition() {
  int available_height = tray_->bubble()->CalculateMaxHeight() -
                         tray_->bubble()->GetCurrentTrayHeight() -
                         kUnifiedMessageCenterBubbleSpacing;

  message_center_view_->SetMaxHeight(available_height);
  message_center_view_->SetAvailableHeight(available_height);

  gfx::Rect resting_bounds = tray_->shelf()->GetSystemTrayAnchorRect();
  resting_bounds.set_y(resting_bounds.y() -
                       tray_->bubble()->GetCurrentTrayHeight() -
                       kUnifiedMessageCenterBubbleSpacing);

  bubble_widget_->SetBounds(resting_bounds);
  bubble_view_->ChangeAnchorRect(resting_bounds);
}

TrayBackgroundView* UnifiedMessageCenterBubble::GetTray() const {
  return tray_;
}

TrayBubbleView* UnifiedMessageCenterBubble::GetBubbleView() const {
  return bubble_view_;
}

views::Widget* UnifiedMessageCenterBubble::GetBubbleWidget() const {
  return bubble_widget_;
}

void UnifiedMessageCenterBubble::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  if (message_center_view_->GetVisible())
    bubble_widget_->Show();
  else
    bubble_widget_->Hide();
}

void UnifiedMessageCenterBubble::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
}

void UnifiedMessageCenterBubble::OnExpandedAmountChanged() {
  UpdatePosition();
  bubble_view_->Layout();
}

}  // namespace ash