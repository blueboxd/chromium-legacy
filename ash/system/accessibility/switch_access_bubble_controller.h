// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_BUBBLE_CONTROLLER_H_

#include "ash/system/tray/tray_bubble_view.h"

namespace ash {

class SwitchAccessBackButtonView;

// Manages the Switch Access back button bubble.
class ASH_EXPORT SwitchAccessBubbleController
    : public TrayBubbleView::Delegate {
 public:
  SwitchAccessBubbleController();
  ~SwitchAccessBubbleController() override;

  SwitchAccessBubbleController(const SwitchAccessBubbleController&) = delete;
  SwitchAccessBubbleController& operator=(const SwitchAccessBubbleController&) =
      delete;

  void ShowBackButton(const gfx::Rect& anchor);
  void CloseBubble();

  SwitchAccessBackButtonView* GetBackButtonViewForTesting() {
    return back_button_view_;
  }

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;

 private:
  // Owned by views hierarchy.
  SwitchAccessBackButtonView* back_button_view_ = nullptr;
  TrayBubbleView* back_button_bubble_view_ = nullptr;

  views::Widget* back_button_widget_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_BUBBLE_CONTROLLER_H_
