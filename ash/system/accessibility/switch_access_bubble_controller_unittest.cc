// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_bubble_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/switch_access_back_button_view.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "ui/accessibility/accessibility_switches.h"

namespace ash {

class SwitchAccessBubbleControllerTest : public AshTestBase {
 public:
  SwitchAccessBubbleControllerTest() = default;
  ~SwitchAccessBubbleControllerTest() override = default;

  SwitchAccessBubbleControllerTest(const SwitchAccessBubbleControllerTest&) =
      delete;
  SwitchAccessBubbleControllerTest& operator=(
      const SwitchAccessBubbleControllerTest&) = delete;

  // testing::Test:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilitySwitchAccess);
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->SetSwitchAccessEnabled(true);
  }

  SwitchAccessBubbleController* GetBubbleController() {
    return Shell::Get()
        ->accessibility_controller()
        ->GetSwitchAccessBubbleControllerForTest();
  }

  gfx::Rect GetBackButtonBounds() {
    SwitchAccessBubbleController* bubble_controller = GetBubbleController();
    if (bubble_controller && bubble_controller->GetBackButtonViewForTesting())
      return bubble_controller->GetBackButtonViewForTesting()
          ->GetBoundsInScreen();
    return gfx::Rect();
  }
};

// TODO(anastasi): Add more tests for closing and repositioning the button.
TEST_F(SwitchAccessBubbleControllerTest, ShowBackButton) {
  EXPECT_TRUE(GetBubbleController());
  gfx::Rect anchor_rect(100, 100, 0, 0);
  GetBubbleController()->ShowBackButton(anchor_rect);

  gfx::Rect bounds = GetBackButtonBounds();
  EXPECT_EQ(bounds.width(), 36);
  EXPECT_EQ(bounds.height(), 36);
}

}  // namespace ash
