// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_accessibility_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"

namespace ash {

namespace {

// A buffer for checking whether the menu view is near this region of the
// screen. This buffer allows for things like padding and the shelf size,
// but is still smaller than half the screen size, so that we can check the
// general corner in which the menu is displayed.
const int kMenuViewBoundsBuffer = 100;

ui::GestureEvent CreateTapEvent() {
  return ui::GestureEvent(0, 0, 0, base::TimeTicks(),
                          ui::GestureEventDetails(ui::ET_GESTURE_TAP));
}

}  // namespace
class FloatingAccessibilityControllerTest : public AshTestBase {
 public:
  AccessibilityControllerImpl* accessibility_controller() {
    return Shell::Get()->accessibility_controller();
  }

  FloatingAccessibilityController* controller() {
    return accessibility_controller()->GetFloatingMenuControllerForTesting();
  }

  FloatingMenuPosition menu_position() { return controller()->position_; }

  FloatingAccessibilityView* menu_view() {
    return controller() ? controller()->menu_view_ : nullptr;
  }

  bool detailed_view_shown() { return controller()->detailed_view_shown_; }

  views::View* GetMenuButton(FloatingAccessibilityView::ButtonId button_id) {
    FloatingAccessibilityView* view = menu_view();
    if (!view)
      return nullptr;
    return view->GetViewByID(static_cast<int>(button_id));
  }

  void SetUpKioskSession() {
    SessionInfo info;
    info.is_running_in_app_mode = true;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }

  gfx::Rect GetMenuViewBounds() {
    FloatingAccessibilityView* view = menu_view();
    return view ? view->GetBoundsInScreen()
                : gfx::Rect(-kMenuViewBoundsBuffer, -kMenuViewBoundsBuffer);
  }

  void Show() { accessibility_controller()->ShowFloatingMenuIfEnabled(); }

  void SetUpVisibleMenu() {
    SetUpKioskSession();
    accessibility_controller()->floating_menu().SetEnabled(true);
    accessibility_controller()->ShowFloatingMenuIfEnabled();
  }
};

TEST_F(FloatingAccessibilityControllerTest, MenuIsNotShownWhenNotEnabled) {
  accessibility_controller()->ShowFloatingMenuIfEnabled();
  EXPECT_EQ(controller(), nullptr);
}

TEST_F(FloatingAccessibilityControllerTest, ShowingMenu) {
  SetUpKioskSession();
  accessibility_controller()->floating_menu().SetEnabled(true);
  accessibility_controller()->ShowFloatingMenuIfEnabled();

  EXPECT_TRUE(controller());
  EXPECT_EQ(menu_position(),
            accessibility_controller()->GetFloatingMenuPosition());
}

TEST_F(FloatingAccessibilityControllerTest, CanChangePosition) {
  SetUpVisibleMenu();

  accessibility_controller()->SetFloatingMenuPosition(
      FloatingMenuPosition::kTopRight);

  // Get the full root window bounds to test the position.
  gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();

  // Test cases rotate clockwise.
  const struct {
    gfx::Point expected_location;
    FloatingMenuPosition expected_position;
  } kTestCases[] = {
      {gfx::Point(window_bounds.width(), window_bounds.height()),
       FloatingMenuPosition::kBottomRight},
      {gfx::Point(0, window_bounds.height()),
       FloatingMenuPosition::kBottomLeft},
      {gfx::Point(0, 0), FloatingMenuPosition::kTopLeft},
      {gfx::Point(window_bounds.width(), 0), FloatingMenuPosition::kTopRight},
  };

  // Find the autoclick menu position button.
  views::View* button =
      GetMenuButton(FloatingAccessibilityView::ButtonId::kPosition);
  ASSERT_TRUE(button) << "No position button found.";

  // Loop through all positions twice.
  for (int i = 0; i < 2; i++) {
    for (const auto& test : kTestCases) {
      SCOPED_TRACE(
          base::StringPrintf("Testing position #[%d]", test.expected_position));
      // Tap the position button.
      ui::GestureEvent event = CreateTapEvent();
      button->OnGestureEvent(&event);

      // Pref change happened.
      EXPECT_EQ(test.expected_position, menu_position());

      // Menu is in generally the correct screen location.
      EXPECT_LT(
          GetMenuViewBounds().ManhattanDistanceToPoint(test.expected_location),
          kMenuViewBoundsBuffer);
    }
  }
}

TEST_F(FloatingAccessibilityControllerTest, DetailedViewToggle) {
  SetUpVisibleMenu();

  // Find the autoclick menu position button.
  views::View* button =
      GetMenuButton(FloatingAccessibilityView::ButtonId::kSettingsList);
  ASSERT_TRUE(button) << "No accessibility features list button found.";
  EXPECT_FALSE(detailed_view_shown());

  ui::GestureEvent event = CreateTapEvent();
  button->OnGestureEvent(&event);
  EXPECT_TRUE(detailed_view_shown());

  event = CreateTapEvent();
  button->OnGestureEvent(&event);
  EXPECT_FALSE(detailed_view_shown());
}

TEST_F(FloatingAccessibilityControllerTest, LocaleChangeObserver) {
  SetUpVisibleMenu();
  gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();

  // RTL should position the menu on the bottom left.
  base::i18n::SetICUDefaultLocale("he");
  // Trigger the LocaleChangeObserver, which should cause a layout of the menu.
  ash::LocaleUpdateController::Get()->OnLocaleChanged(
      "en", "en", "he", base::DoNothing::Once<ash::LocaleNotificationResult>());
  EXPECT_TRUE(base::i18n::IsRTL());
  EXPECT_LT(
      GetMenuViewBounds().ManhattanDistanceToPoint(window_bounds.bottom_left()),
      kMenuViewBoundsBuffer);

  // LTR should position the menu on the bottom right.
  base::i18n::SetICUDefaultLocale("en");
  ash::LocaleUpdateController::Get()->OnLocaleChanged(
      "he", "he", "en", base::DoNothing::Once<ash::LocaleNotificationResult>());
  EXPECT_FALSE(base::i18n::IsRTL());
  EXPECT_LT(GetMenuViewBounds().ManhattanDistanceToPoint(
                window_bounds.bottom_right()),
            kMenuViewBoundsBuffer);
}

}  // namespace ash
