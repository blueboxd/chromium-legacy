// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_SCROLL_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_SCROLL_VIEW_H_

#include "ash/autoclick/autoclick_controller.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class AutoclickScrollCloseButton;
class AutoclickScrollButton;

// View for the Automatic Clicks scroll bubble, which holds the Automatic Clicks
// scroll menu.
class AutoclickScrollBubbleView : public TrayBubbleView {
 public:
  explicit AutoclickScrollBubbleView(TrayBubbleView::InitParams init_params);
  ~AutoclickScrollBubbleView() override;

  void UpdateAnchorRect(const gfx::Rect& rect);

  // TrayBubbleView:
  bool IsAnchoredToStatusArea() const override;

  // views::View:
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoclickScrollBubbleView);
};

// View for the Automatic Clicks scroll menu, which creates and manages
// individual buttons to control Automatic Clicks scrolling.
class AutoclickScrollView : public views::View {
 public:
  // Used for testing. Start at 1 because a view IDs should not be 0.
  enum class ButtonId {
    kScrollUp = 1,
    kScrollDown,
    kScrollLeft,
    kScrollRight,
    kCloseScroll,
  };

  // The amount of time to wait during a hover over a scroll pad button before
  // requesting that Autoclick Controller perform a scroll. Decreasing this
  // below about 100 causes a smooth scroll due to the amount of time scroll
  // animation takes. Visible for tests.
  const static int kAutoclickScrollDelayMs = 250;

  AutoclickScrollView();
  ~AutoclickScrollView() override = default;

 private:
  // views::View:
  void Layout() override;

  // Unowned. Owned by views hierarchy.
  AutoclickScrollButton* scroll_up_button_;
  AutoclickScrollButton* scroll_down_button_;
  AutoclickScrollButton* scroll_left_button_;
  AutoclickScrollButton* scroll_right_button_;
  AutoclickScrollCloseButton* close_scroll_button_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickScrollView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_SCROLL_VIEW_H_
