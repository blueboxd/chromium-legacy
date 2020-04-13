// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_MENU_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_MENU_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class SwitchAccessMenuButton;

// View for the Switch Access menu.
class SwitchAccessMenuView : public views::View, public views::ButtonListener {
 public:
  SwitchAccessMenuView();
  ~SwitchAccessMenuView() override = default;

  SwitchAccessMenuView(const SwitchAccessMenuView&) = delete;
  SwitchAccessMenuView& operator=(const SwitchAccessMenuView&) = delete;

  int GetBubbleWidthDip() const;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  const char* GetClassName() const override;

 private:
  friend class SwitchAccessMenuBubbleControllerTest;

  // Owned by the views hierarchy.
  SwitchAccessMenuButton* scroll_down_button_;
  SwitchAccessMenuButton* select_button_;
  SwitchAccessMenuButton* settings_button_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_MENU_VIEW_H_
