// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/accessibility/autoclick_scroll_bubble_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "ui/accessibility/accessibility_switches.h"

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

class AutoclickMenuBubbleControllerTest : public AshTestBase {
 public:
  AutoclickMenuBubbleControllerTest() = default;
  ~AutoclickMenuBubbleControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalAccessibilityAutoclick);
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->SetAutoclickEnabled(true);
  }

  AutoclickMenuBubbleController* GetBubbleController() {
    return Shell::Get()
        ->autoclick_controller()
        ->GetMenuBubbleControllerForTesting();
  }

  AutoclickMenuView* GetMenuView() {
    return GetBubbleController() ? GetBubbleController()->menu_view_ : nullptr;
  }

  views::View* GetMenuButton(AutoclickMenuView::ButtonId view_id) {
    AutoclickMenuView* menu_view = GetMenuView();
    if (!menu_view)
      return nullptr;
    return menu_view->GetViewByID(static_cast<int>(view_id));
  }

  gfx::Rect GetMenuViewBounds() {
    return GetBubbleController()
               ? GetBubbleController()->menu_view_->GetBoundsInScreen()
               : gfx::Rect(-kMenuViewBoundsBuffer, -kMenuViewBoundsBuffer);
  }

  AutoclickScrollView* GetScrollView() {
    return GetBubbleController()->scroll_bubble_controller_
               ? GetBubbleController()->scroll_bubble_controller_->scroll_view_
               : nullptr;
  }

  views::View* GetScrollButton(AutoclickScrollView::ButtonId view_id) {
    AutoclickScrollView* scroll_view = GetScrollView();
    if (!scroll_view)
      return nullptr;
    return scroll_view->GetViewByID(static_cast<int>(view_id));
  }

  gfx::Rect GetScrollViewBounds() {
    return GetBubbleController()->scroll_bubble_controller_
               ? GetBubbleController()
                     ->scroll_bubble_controller_->scroll_view_
                     ->GetBoundsInScreen()
               : gfx::Rect(-kMenuViewBoundsBuffer, -kMenuViewBoundsBuffer);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoclickMenuBubbleControllerTest);
};

TEST_F(AutoclickMenuBubbleControllerTest, ExistsOnlyWhenAutoclickIsRunning) {
  // Cycle a few times to ensure there are no crashes when toggling the feature.
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(GetBubbleController());
    EXPECT_TRUE(GetMenuView());
    Shell::Get()->autoclick_controller()->SetEnabled(
        false, false /* do not show dialog */);
    EXPECT_FALSE(GetBubbleController());
    Shell::Get()->autoclick_controller()->SetEnabled(
        true, false /* do not show dialog */);
  }
}

TEST_F(AutoclickMenuBubbleControllerTest, CanSelectAutoclickTypeFromBubble) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  // Set to a different event type than the first event in kTestCases.
  controller->SetAutoclickEventType(mojom::AutoclickEventType::kRightClick);

  const struct {
    AutoclickMenuView::ButtonId view_id;
    mojom::AutoclickEventType expected_event_type;
  } kTestCases[] = {
      {AutoclickMenuView::ButtonId::kLeftClick,
       mojom::AutoclickEventType::kLeftClick},
      {AutoclickMenuView::ButtonId::kRightClick,
       mojom::AutoclickEventType::kRightClick},
      {AutoclickMenuView::ButtonId::kDoubleClick,
       mojom::AutoclickEventType::kDoubleClick},
      {AutoclickMenuView::ButtonId::kDragAndDrop,
       mojom::AutoclickEventType::kDragAndDrop},
      {AutoclickMenuView::ButtonId::kScroll,
       mojom::AutoclickEventType::kScroll},
      {AutoclickMenuView::ButtonId::kPause,
       mojom::AutoclickEventType::kNoAction},
  };

  for (const auto& test : kTestCases) {
    // Find the autoclick action button for this test case.
    views::View* button = GetMenuButton(test.view_id);
    ASSERT_TRUE(button) << "No view for id " << static_cast<int>(test.view_id);

    // Tap the action button.
    ui::GestureEvent event = CreateTapEvent();
    button->OnGestureEvent(&event);

    // Pref change happened.
    EXPECT_EQ(test.expected_event_type, controller->GetAutoclickEventType());
  }
}

TEST_F(AutoclickMenuBubbleControllerTest, UnpausesWhenPauseAlreadySelected) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  views::View* pause_button =
      GetMenuButton(AutoclickMenuView::ButtonId::kPause);
  ui::GestureEvent event = CreateTapEvent();

  const struct {
    mojom::AutoclickEventType event_type;
  } kTestCases[]{
      {mojom::AutoclickEventType::kRightClick},
      {mojom::AutoclickEventType::kLeftClick},
      {mojom::AutoclickEventType::kDoubleClick},
      {mojom::AutoclickEventType::kDragAndDrop},
      {mojom::AutoclickEventType::kScroll},
  };

  for (const auto& test : kTestCases) {
    controller->SetAutoclickEventType(test.event_type);

    // First tap pauses.
    pause_button->OnGestureEvent(&event);
    EXPECT_EQ(mojom::AutoclickEventType::kNoAction,
              controller->GetAutoclickEventType());

    // Second tap unpauses and reverts to previous state.
    pause_button->OnGestureEvent(&event);
    EXPECT_EQ(test.event_type, controller->GetAutoclickEventType());
  }
}

TEST_F(AutoclickMenuBubbleControllerTest, CanChangePosition) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();

  // Set to a known position for than the first event in kTestCases.
  controller->SetAutoclickMenuPosition(mojom::AutoclickMenuPosition::kTopRight);

  // Get the full root window bounds to test the position.
  gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();

  // Test cases rotate clockwise.
  const struct {
    gfx::Point expected_location;
    mojom::AutoclickMenuPosition expected_position;
  } kTestCases[] = {
      {gfx::Point(window_bounds.width(), window_bounds.height()),
       mojom::AutoclickMenuPosition::kBottomRight},
      {gfx::Point(0, window_bounds.height()),
       mojom::AutoclickMenuPosition::kBottomLeft},
      {gfx::Point(0, 0), mojom::AutoclickMenuPosition::kTopLeft},
      {gfx::Point(window_bounds.width(), 0),
       mojom::AutoclickMenuPosition::kTopRight},
  };

  // Find the autoclick menu position button.
  views::View* button = GetMenuButton(AutoclickMenuView::ButtonId::kPosition);
  ASSERT_TRUE(button) << "No position button found.";

  // Loop through all positions twice.
  for (int i = 0; i < 2; i++) {
    for (const auto& test : kTestCases) {
      // Tap the position button.
      ui::GestureEvent event = CreateTapEvent();
      button->OnGestureEvent(&event);

      // Pref change happened.
      EXPECT_EQ(test.expected_position, controller->GetAutoclickMenuPosition());

      // Menu is in generally the correct screen location.
      EXPECT_LT(
          GetMenuViewBounds().ManhattanDistanceToPoint(test.expected_location),
          kMenuViewBoundsBuffer);
    }
  }
}

TEST_F(AutoclickMenuBubbleControllerTest, DefaultChangesWithTextDirection) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();

  // RTL should position the menu on the bottom left.
  base::i18n::SetRTLForTesting(true);
  // Force a layout.
  controller->UpdateAutoclickMenuBoundsIfNeeded();
  EXPECT_LT(
      GetMenuViewBounds().ManhattanDistanceToPoint(window_bounds.bottom_left()),
      kMenuViewBoundsBuffer);

  // LTR should position the menu on the bottom right.
  base::i18n::SetRTLForTesting(false);
  // Force a layout.
  controller->UpdateAutoclickMenuBoundsIfNeeded();
  EXPECT_LT(GetMenuViewBounds().ManhattanDistanceToPoint(
                window_bounds.bottom_right()),
            kMenuViewBoundsBuffer);
}

TEST_F(AutoclickMenuBubbleControllerTest, ScrollBubbleShowsAndCloses) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetAutoclickEventType(mojom::AutoclickEventType::kLeftClick);
  // No scroll view yet.
  EXPECT_FALSE(GetScrollView());

  // Scroll type should cause the scroll view to be shown.
  controller->SetAutoclickEventType(mojom::AutoclickEventType::kScroll);
  EXPECT_TRUE(GetScrollView());

  // Clicking the scroll close button resets to left click.
  views::View* close_button =
      GetScrollButton(AutoclickScrollView::ButtonId::kCloseScroll);
  ui::GestureEvent event = CreateTapEvent();
  close_button->OnGestureEvent(&event);
  EXPECT_FALSE(GetScrollView());
  EXPECT_EQ(mojom::AutoclickEventType::kLeftClick,
            controller->GetAutoclickEventType());
}

TEST_F(AutoclickMenuBubbleControllerTest, ScrollBubblePositioning) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetAutoclickEventType(mojom::AutoclickEventType::kScroll);

  const struct { bool is_RTL; } kTestCases[] = {{true}, {false}};
  for (auto& test : kTestCases) {
    // These positions should be relative to the corners of the screen
    // whether we are in RTL or LTR.
    base::i18n::SetRTLForTesting(test.is_RTL);

    // When the menu is in the top right, the scroll view should be directly
    // under it and along the right side of the screen.
    controller->SetAutoclickMenuPosition(
        mojom::AutoclickMenuPosition::kTopRight);
    EXPECT_LT(GetScrollViewBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().bottom_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetScrollViewBounds().right(), GetMenuViewBounds().right());

    // When the menu is in the bottom right, the scroll view is directly above
    // it and along the right side of the screen.
    controller->SetAutoclickMenuPosition(
        mojom::AutoclickMenuPosition::kBottomRight);
    EXPECT_LT(GetScrollViewBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().top_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetScrollViewBounds().right(), GetMenuViewBounds().right());

    // When the menu is on the bottom left, the scroll view is directly above it
    // and along the left side of the screen.
    controller->SetAutoclickMenuPosition(
        mojom::AutoclickMenuPosition::kBottomLeft);
    EXPECT_LT(GetScrollViewBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().top_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetScrollViewBounds().x(), GetMenuViewBounds().x());

    // When the menu is on the top left, the scroll view is directly below it
    // and along the left side of the screen.
    controller->SetAutoclickMenuPosition(
        mojom::AutoclickMenuPosition::kTopLeft);
    EXPECT_LT(GetScrollViewBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().bottom_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetScrollViewBounds().x(), GetMenuViewBounds().x());
  }
}

}  // namespace ash
