// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_BUBBLE_CONTROLLER_H_

#include "ash/public/cpp/ash_constants.h"
#include "ash/system/accessibility/autoclick_menu_view.h"
#include "ash/system/locale/locale_update_controller_impl.h"
#include "ash/system/tray/tray_bubble_view.h"

namespace ash {

class AutoclickScrollBubbleController;

// Manages the bubble which contains an AutoclickMenuView.
class AutoclickMenuBubbleController : public TrayBubbleView::Delegate,
                                      public LocaleChangeObserver {
 public:
  AutoclickMenuBubbleController();
  ~AutoclickMenuBubbleController() override;

  // Sets the currently selected event type.
  void SetEventType(mojom::AutoclickEventType type);

  // Sets the menu's position on the screen.
  void SetPosition(mojom::AutoclickMenuPosition position);

  void ShowBubble(mojom::AutoclickEventType event_type,
                  mojom::AutoclickMenuPosition position);

  void CloseBubble();

  // Shows or hides the bubble.
  void SetBubbleVisibility(bool is_visible);

  // Performs a mouse event on the bubble at the given location in DIPs.
  void ClickOnBubble(gfx::Point location_in_dips, int mouse_event_flags);

  // Performs a mouse event on the scroll bubble at the given location in DIPs.
  void ClickOnScrollBubble(gfx::Point location_in_dips, int mouse_event_flags);

  // Whether the the bubble, if the bubble exists, contains the given screen
  // point.
  bool ContainsPointInScreen(const gfx::Point& point);

  // Whether the scroll bubble exists and contains the given screen point.
  bool ScrollBubbleContainsPointInScreen(const gfx::Point& point);

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;

  // LocaleChangeObserver:
  void OnLocaleChanged() override;

 private:
  friend class AutoclickMenuBubbleControllerTest;
  friend class AutoclickTest;

  // Owned by views hierarchy.
  AutoclickMenuBubbleView* bubble_view_ = nullptr;
  AutoclickMenuView* menu_view_ = nullptr;
  mojom::AutoclickMenuPosition position_ = kDefaultAutoclickMenuPosition;

  views::Widget* bubble_widget_ = nullptr;

  // The controller for the scroll bubble. Only exists during a scroll. Owned
  // by this class so that positioning calculations can take place using both
  // classes at once.
  std::unique_ptr<AutoclickScrollBubbleController> scroll_bubble_controller_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickMenuBubbleController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_BUBBLE_CONTROLLER_H_
