// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/test/shelf_layout_manager_test_base.h"
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

}  // namespace ash
