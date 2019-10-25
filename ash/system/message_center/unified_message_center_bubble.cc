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
#include "ash/system/unified/unified_system_tray_view.h"
#include "ui/views/focus/focus_search.h"
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
  init_params.close_on_deactivate = false;

  bubble_view_ = new TrayBubbleView(init_params);

  message_center_view_ =
      bubble_view_->AddChildView(std::make_unique<UnifiedMessageCenterView>(
          nullptr /* parent */, tray->model(), this));

  // Check if the message center bubble should be collapsed when it is initially
  // opened.
  if (CalculateAvailableHeight() < kMessageCenterCollapseThreshold)
    message_center_view_->SetCollapsed(false /*animate*/);

  message_center_view_->AddObserver(this);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);

  bubble_view_->InitializeAndShowBubble();

  tray->tray_event_filter()->AddBubble(this);
  tray->bubble()->unified_view()->AddObserver(this);

  UpdatePosition();
}

UnifiedMessageCenterBubble::~UnifiedMessageCenterBubble() {
  if (bubble_widget_) {
    tray_->tray_event_filter()->RemoveBubble(this);
    tray_->bubble()->unified_view()->RemoveObserver(this);
    CHECK(message_center_view_);
    message_center_view_->RemoveObserver(this);

    bubble_widget_->RemoveObserver(this);
    bubble_widget_->CloseNow();
  }
}

int UnifiedMessageCenterBubble::CalculateAvailableHeight() {
  return tray_->bubble()->CalculateMaxHeight() -
         tray_->bubble()->GetCurrentTrayHeight() -
         kUnifiedMessageCenterBubbleSpacing;
}

void UnifiedMessageCenterBubble::CollapseMessageCenter() {
  message_center_view_->SetCollapsed(true /*animate*/);
}

void UnifiedMessageCenterBubble::ExpandMessageCenter() {
  message_center_view_->SetExpanded();
  UpdatePosition();
  tray_->EnsureQuickSettingsCollapsed();
}

void UnifiedMessageCenterBubble::UpdatePosition() {
  int available_height = CalculateAvailableHeight();

  message_center_view_->SetMaxHeight(available_height);
  message_center_view_->SetAvailableHeight(available_height);

  // Note: It's tempting to set the insets for TrayBubbleView in order to
  // achieve the padding, but that enlarges the layer bounds and breaks rounded
  // corner clipping for ARC notifications. This approach only modifies the
  // position of the layer.
  gfx::Rect resting_bounds = tray_->shelf()->GetSystemTrayAnchorRect();
  resting_bounds.set_x(resting_bounds.x() - kUnifiedMenuPadding);
  resting_bounds.set_y(
      resting_bounds.y() - tray_->bubble()->GetCurrentTrayHeight() -
      kUnifiedMenuPadding - kUnifiedMessageCenterBubbleSpacing);

  bubble_widget_->SetBounds(resting_bounds);
  bubble_view_->ChangeAnchorRect(resting_bounds);
}

void UnifiedMessageCenterBubble::FocusEntered(bool reverse) {
  message_center_view_->FocusEntered(reverse);
}

bool UnifiedMessageCenterBubble::FocusOut(bool reverse) {
  return tray_->FocusQuickSettings(reverse);
}

void UnifiedMessageCenterBubble::FocusFirstNotification() {
  message_center_view_->GetFocusManager()->AdvanceFocus(false /*reverse*/);
}

bool UnifiedMessageCenterBubble::IsMessageCenterVisible() {
  return message_center_view_->GetVisible();
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

void UnifiedMessageCenterBubble::OnViewPreferredSizeChanged(
    views::View* observed_view) {
  UpdatePosition();
  bubble_view_->Layout();
}

void UnifiedMessageCenterBubble::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  tray_->tray_event_filter()->RemoveBubble(this);
  tray_->bubble()->unified_view()->RemoveObserver(this);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
}

}  // namespace ash
