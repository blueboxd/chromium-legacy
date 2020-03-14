// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/test/shelf_layout_manager_test_base.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"

namespace ash {
namespace {

ShelfWidget* GetShelfWidget() {
  return AshTestBase::GetPrimaryShelf()->shelf_widget();
}

}  // namespace

// Test base for unit test related to drag handle contextual nudges.
class DragHandleContextualNudgeTest : public ShelfLayoutManagerTestBase {
 public:
  DragHandleContextualNudgeTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kContextualNudges);
  }
  ~DragHandleContextualNudgeTest() override = default;

  DragHandleContextualNudgeTest(const DragHandleContextualNudgeTest& other) =
      delete;
  DragHandleContextualNudgeTest& operator=(
      const DragHandleContextualNudgeTest& other) = delete;

  // ShelfLayoutManagerTestBase:
  void SetUp() override {
    ShelfLayoutManagerTestBase::SetUp();
    test_clock_.Advance(base::TimeDelta::FromHours(2));
    contextual_tooltip::OverrideClockForTesting(&test_clock_);
  }
  void TearDown() override {
    contextual_tooltip::ClearClockOverrideForTesting();
    AshTestBase::TearDown();
  }

  base::SimpleTestClock test_clock_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DragHandleContextualNudgeTest, ShowDragHandleNudgeWithTimer) {
  // Creates a widget that will become maximized in tablet mode.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp, GetShelfWidget()->GetBackgroundType());

  // The drag handle should be showing but the nudge should not. A timer to show
  // the nudge should be initialized.
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  // Firing the timer should show the drag handle nudge.
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
}

TEST_F(DragHandleContextualNudgeTest, HideDragHandleNudgeHiddenOnMinimize) {
  // Creates a widget that will become maximized in tablet mode.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp, GetShelfWidget()->GetBackgroundType());

  // The drag handle and nudge should be showing after the timer fires.
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Minimizing the widget should hide the drag handle and nudge.
  widget->Minimize();
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
}

TEST_F(DragHandleContextualNudgeTest, DoNotShowNudgeWithoutDragHandle) {
  // Creates a widget that will become maximized in tablet mode.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp, GetShelfWidget()->GetBackgroundType());

  // Minimizing the widget should hide the drag handle and nudge.
  widget->Minimize();
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
}

TEST_F(DragHandleContextualNudgeTest,
       ContinueShowingDragHandleNudgeOnActiveWidgetChanged) {
  // Creates a widget that will become maximized in tablet mode.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp, GetShelfWidget()->GetBackgroundType());
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Maximizing and showing a different widget should not hide the drag handle
  // or nudge.
  views::Widget* new_widget = CreateTestWidget();
  new_widget->Maximize();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
}

TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeShownInAppShelf) {
  // Creates a widget that will become maximized in tablet mode.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  // Drag handle and nudge should not be shown in clamshell mode.
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Test that the first time a user transitions into tablet mode with a
  // maximized window will show the drag nudge immedietly. The drag handle nudge
  // should not be visible yet and the timer to show it should be set.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp, GetShelfWidget()->GetBackgroundType());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  EXPECT_TRUE(GetShelfWidget()
                  ->GetDragHandle()
                  ->has_show_drag_handle_timer_for_testing());

  // Firing the timer should show the nudge for the first time. The nudge should
  // remain visible until the shelf state changes so the timer to hide it should
  // not be set.
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetDragHandle()
                   ->has_hide_drag_handle_timer_for_testing());

  // Leaving tablet mode should hide the nudge.
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Reentering tablet mode should show the drag handle but the nudge should
  // not. No timer should be set to show the nudge.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetDragHandle()
                   ->has_show_drag_handle_timer_for_testing());

  // Advance time for more than a day (which should enable the nudge again).
  test_clock_.Advance(base::TimeDelta::FromHours(25));

  // Reentering tablet mode with a maximized widget should immedietly show the
  // drag handle and set a timer to show the nudge.
  TabletModeControllerTestApi().LeaveTabletMode();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  // Firing the timer should show the nudge.
  EXPECT_TRUE(GetShelfWidget()
                  ->GetDragHandle()
                  ->has_show_drag_handle_timer_for_testing());
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetDragHandle()
                   ->has_show_drag_handle_timer_for_testing());
  // On subsequent shows, the nudge should be hidden after a timeout.
  EXPECT_TRUE(GetShelfWidget()
                  ->GetDragHandle()
                  ->has_hide_drag_handle_timer_for_testing());
}

TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeShownOnTap) {
  // Creates a widget that will become maximized in tablet mode.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp, GetShelfWidget()->GetBackgroundType());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  EXPECT_TRUE(GetShelfWidget()
                  ->GetDragHandle()
                  ->has_show_drag_handle_timer_for_testing());
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Exiting and re-entering tablet should hide the nudge and put the shelf into
  // the default kInApp shelf state.
  TabletModeControllerTestApi().LeaveTabletMode();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Tapping the drag handle should show the drag handle nudge immedietly and
  // the show nudge timer should be set.
  GetEventGenerator()->GestureTapAt(
      GetShelfWidget()->GetDragHandle()->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetDragHandle()
                   ->has_show_drag_handle_timer_for_testing());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  EXPECT_TRUE(GetShelfWidget()
                  ->GetDragHandle()
                  ->has_hide_drag_handle_timer_for_testing());
}

TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeNotShownForHiddenShelf) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget that will become maximized in tablet mode.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  // The shelf is hidden, so the drag handle nudge should not be shown.
  EXPECT_TRUE(drag_handle->GetVisible());
  EXPECT_FALSE(drag_handle->ShowingNudge());
  EXPECT_FALSE(drag_handle->has_show_drag_handle_timer_for_testing());

  PrefService* const prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  // Back gesture nudge should be allowed if the shelf is hidden.
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      prefs, contextual_tooltip::TooltipType::kBackGesture, nullptr));

  // Swipe up to show the shelf - this should schedule the drag handle nudge.
  SwipeUpOnShelf();

  // Back gesture nudge should be disallowed at this time, given that the drag
  // handle nudge can be shown.
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      prefs, contextual_tooltip::TooltipType::kBackGesture, nullptr));

  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->ShowingNudge());
}

// Tests that the drag handle nudge is horizontally centered in screen, and
// drawn above the shelf drag handle, even after display bounds are updated.
TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeBoundsInScreen) {
  UpdateDisplay("675x1200");
  TabletModeControllerTestApi().EnterTabletMode();

  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  EXPECT_TRUE(drag_handle->GetVisible());
  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->ShowingNudge());

  // Calculates absolute difference between horizontal margins of |inner| rect
  // within |outer| rect.
  auto margin_diff = [](const gfx::Rect& inner, const gfx::Rect& outer) -> int {
    const int left = inner.x() - outer.x();
    EXPECT_GE(left, 0);

    const int right = outer.right() - inner.right();
    EXPECT_GE(right, 0);

    return std::abs(left - right);
  };

  // Verify that nudge widget is centered in shelf.
  gfx::Rect shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  gfx::Rect nudge_bounds = drag_handle->drag_handle_nudge_for_testing()
                               ->label()
                               ->GetBoundsInScreen();
  EXPECT_LE(margin_diff(nudge_bounds, shelf_bounds), 1);

  // Verify that the nudge vertical bounds - within the shelf bounds, and above
  // the drag handle.
  gfx::Rect drag_handle_bounds = drag_handle->GetBoundsInScreen();
  EXPECT_LE(shelf_bounds.y(), nudge_bounds.y());
  EXPECT_LE(nudge_bounds.bottom(), drag_handle_bounds.y());

  // Change the display bounds, and verify the updated drag handle bounds.
  UpdateDisplay("1200x675");
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Verify that nudge widget is centered in shelf.
  shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  nudge_bounds = drag_handle->drag_handle_nudge_for_testing()
                     ->label()
                     ->GetBoundsInScreen();
  EXPECT_LE(margin_diff(nudge_bounds, shelf_bounds), 1);

  // Verify that the nudge vertical bounds - within the shelf bounds, and above
  // the drag handle.
  drag_handle_bounds = drag_handle->GetBoundsInScreen();
  EXPECT_LE(shelf_bounds.y(), nudge_bounds.y());
  EXPECT_LE(nudge_bounds.bottom(), drag_handle_bounds.y());
}

}  // namespace ash
