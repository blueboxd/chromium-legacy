// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_DETAILED_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_DETAILED_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ui/views/bubble/bubble_border.h"

namespace ash {

namespace tray {
class AccessibilityDetailedView;
}

// Controller for the detailed view of accessibility floating menu.
class ASH_EXPORT FloatingAccessibilityDetailedController
    : public TrayBubbleView::Delegate,
      public DetailedViewDelegate {
 public:
  class Delegate {
   public:
    virtual void OnDetailedMenuClosed() {}
    virtual ~Delegate() = default;
  };

  explicit FloatingAccessibilityDetailedController(Delegate* delegate);
  ~FloatingAccessibilityDetailedController() override;

  void Show(gfx::Rect anchor_rect, views::BubbleBorder::Arrow alignment);
  void UpdateAnchorRect(gfx::Rect anchor_rect,
                        views::BubbleBorder::Arrow alignment);
  // DetailedViewDelegate:
  void CloseBubble() override;
  void TransitionToMainView(bool restore_focus) override;

  void OnAccessibilityStatusChanged();

 private:
  friend class FloatingAccessibilityControllerTest;
  class DetailedBubbleView;
  // DetailedViewDelegate:
  views::Button* CreateHelpButton(views::ButtonListener* listener) override;
  views::Button* CreateSettingsButton(views::ButtonListener* listener,
                                      int setting_accessible_name_id) override;
  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;

  DetailedBubbleView* bubble_view_ = nullptr;
  views::Widget* bubble_widget_ = nullptr;
  tray::AccessibilityDetailedView* detailed_view_ = nullptr;

  Delegate* const delegate_;  // Owns us.
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_DETAILED_CONTROLLER_H
