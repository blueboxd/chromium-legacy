// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_menu_bubble_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/switch_access_back_button_bubble_controller.h"
#include "ash/system/accessibility/switch_access_back_button_view.h"
#include "ash/system/accessibility/switch_access_menu_button.h"
#include "ash/system/accessibility/switch_access_menu_view.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "ui/accessibility/accessibility_switches.h"

namespace ash {

class SwitchAccessMenuBubbleControllerTest : public AshTestBase {
 public:
  SwitchAccessMenuBubbleControllerTest() = default;
  ~SwitchAccessMenuBubbleControllerTest() override = default;

  SwitchAccessMenuBubbleControllerTest(
      const SwitchAccessMenuBubbleControllerTest&) = delete;
  SwitchAccessMenuBubbleControllerTest& operator=(
      const SwitchAccessMenuBubbleControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilitySwitchAccess);
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->SetSwitchAccessEnabled(true);
  }

  SwitchAccessMenuBubbleController* GetBubbleController() {
    return Shell::Get()
        ->accessibility_controller()
        ->GetSwitchAccessBubbleControllerForTest();
  }

  SwitchAccessMenuView* GetMenuView() {
    return GetBubbleController()->menu_view_;
  }

  std::vector<SwitchAccessMenuButton*> GetMenuButtons() {
    return std::vector<SwitchAccessMenuButton*>(
        {GetMenuView()->scroll_down_button_, GetMenuView()->select_button_,
         GetMenuView()->settings_button_});
  }

  gfx::Rect GetBackButtonBounds() {
    SwitchAccessMenuBubbleController* bubble_controller = GetBubbleController();
    if (bubble_controller && bubble_controller->back_button_controller_ &&
        bubble_controller->back_button_controller_->back_button_view_) {
      return bubble_controller->back_button_controller_->back_button_view_
          ->GetBoundsInScreen();
    }
    return gfx::Rect();
  }

  int GetExpectedButtonWidth() { return SwitchAccessMenuButton::kWidthDip; }
  int GetExpectedBubbleWidth() {
    return 288;  // From the Switch Access spec.
  }
};

// TODO(anastasi): Add more tests for closing and repositioning the button.
TEST_F(SwitchAccessMenuBubbleControllerTest, ShowBackButton) {
  EXPECT_TRUE(GetBubbleController());
  gfx::Rect anchor_rect(100, 100, 0, 0);
  GetBubbleController()->ShowBackButton(anchor_rect);

  gfx::Rect bounds = GetBackButtonBounds();
  EXPECT_EQ(bounds.width(), 36);
  EXPECT_EQ(bounds.height(), 36);
}

TEST_F(SwitchAccessMenuBubbleControllerTest, ShowMenu) {
  EXPECT_TRUE(GetBubbleController());
  gfx::Rect anchor_rect(10, 10, 0, 0);
  GetBubbleController()->ShowMenu(anchor_rect);
  EXPECT_TRUE(GetMenuView());

  for (SwitchAccessMenuButton* button : GetMenuButtons()) {
    EXPECT_TRUE(button);
    EXPECT_EQ(button->width(), GetExpectedButtonWidth());
  }

  EXPECT_EQ(GetMenuView()->width(), GetExpectedBubbleWidth());
}

}  // namespace ash
