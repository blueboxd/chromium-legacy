// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_SCROLL_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_SCROLL_BUBBLE_CONTROLLER_H_

#include "ash/public/cpp/ash_constants.h"
#include "ash/system/accessibility/autoclick_scroll_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ui/views/bubble/bubble_border.h"

namespace ash {

// Manages the bubble which contains an AutoclickScrollView.
class AutoclickScrollBubbleController : public TrayBubbleView::Delegate {
 public:
  AutoclickScrollBubbleController();
  ~AutoclickScrollBubbleController() override;

  void UpdateAnchorRect(gfx::Rect rect, views::BubbleBorder::Arrow alignment);

  void ShowBubble(gfx::Rect anchor_rect, views::BubbleBorder::Arrow alignment);
  void CloseBubble();

  // Shows or hides the bubble.
  void SetBubbleVisibility(bool is_visible);

  // Performs the mouse events on the bubble. at the given location in DIPs.
  void ClickOnBubble(gfx::Point location_in_dips, int mouse_event_flags);

  // Whether the tray button or the bubble, if the bubble exists, contain
  // the given screen point.
  bool ContainsPointInScreen(const gfx::Point& point);

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;

 private:
  friend class AutoclickMenuBubbleControllerTest;
  friend class AutoclickTest;

  // Owned by views hierarchy.
  AutoclickScrollBubbleView* bubble_view_ = nullptr;
  AutoclickScrollView* scroll_view_ = nullptr;
  views::Widget* bubble_widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AutoclickScrollBubbleController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_SCROLL_BUBBLE_CONTROLLER_H_
