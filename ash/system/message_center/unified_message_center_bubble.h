// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_BUBBLE_H_
#define ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_BUBBLE_H_

#include "ash/system/tray/tray_bubble_base.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class TrayBubbleView;
class UnifiedSystemTray;
class UnifiedMessageCenterView;

// Manages the bubble that contains UnifiedMessageCenterView.
// Shows the bubble on the constructor, and closes the bubble on the destructor.
class ASH_EXPORT UnifiedMessageCenterBubble : public TrayBubbleBase,
                                              public views::ViewObserver,
                                              public views::WidgetObserver {
 public:
  explicit UnifiedMessageCenterBubble(UnifiedSystemTray* tray);
  ~UnifiedMessageCenterBubble() override;

  void UpdatePosition();

  // TrayBubbleBase:
  TrayBackgroundView* GetTray() const override;
  TrayBubbleView* GetBubbleView() const override;
  views::Widget* GetBubbleWidget() const override;

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Called when the system tray expanded amount is changed. Required to
  // position the message center right above the tray when it is moving.
  void OnExpandedAmountChanged();

 private:
  UnifiedSystemTray* const tray_;

  views::Widget* bubble_widget_ = nullptr;
  TrayBubbleView* bubble_view_ = nullptr;
  UnifiedMessageCenterView* message_center_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageCenterBubble);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_BUBBLE_H_
