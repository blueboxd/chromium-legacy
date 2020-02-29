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

TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeShownInAppShelf) {
  // Creates a widget that will become maximized in tablet mode.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  // Drag handle and nudge should not be shown in clamshell mode.
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  TabletModeControllerTestApi().EnterTabletMode();

  // Test that the first time a user transitioning into tablet mode a maximized
  // window will show the drag handle nudge. This drag handle nudge should not
  // have a timeout.
  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_EQ(ShelfBackgroundType::kInApp, GetShelfWidget()->GetBackgroundType());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->HasHideTimerForTesting());

  // The nudge should remain visible until the shelf state changes.
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Reentering tablet mode should show the drag handle but not the nudge.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Advance time for more than a day (which should enable the nudge again).
  test_clock_.Advance(base::TimeDelta::FromHours(25));
  TabletModeControllerTestApi().LeaveTabletMode();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  // On subsequent shows, the nudge should be hidden after a timeout.
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->HasHideTimerForTesting());
}

TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeShownOnTap) {
  // Creates a widget that will become fullscreen in tablet mode.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  // The drag handle and nudge should be showing.
  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_EQ(ShelfBackgroundType::kInApp, GetShelfWidget()->GetBackgroundType());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Exiting and re-entering tablet should hide the nudge and put the shelf into
  // the default kInApp shelf state.
  TabletModeControllerTestApi().LeaveTabletMode();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  // Tapping the drag handle should show the drag handle nudge.

  GetEventGenerator()->GestureTapAt(
      GetShelfWidget()->GetDragHandle()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->HasHideTimerForTesting());
}

TEST_F(DragHandleContextualNudgeTest, HideDragHandleNudge) {
  // Creates a widget that will become fullscreen in tablet mode.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();

  // The drag handle and nudge should be showing.
  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_EQ(ShelfBackgroundType::kInApp, GetShelfWidget()->GetBackgroundType());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Hiding the drag handle nudge should not affect the visibility of the drag
  // handle.
  GetShelfWidget()->GetDragHandle()->HideDragHandleNudge();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
}

TEST_F(DragHandleContextualNudgeTest, HideDragHandleNudgeHiddenOnMinimize) {
  // Creates a widget that will become fullscreen in tablet mode.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();

  // The drag handle and nudge should be showing.
  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_EQ(ShelfBackgroundType::kInApp, GetShelfWidget()->GetBackgroundType());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->ShowingNudge());

  // Minimizing the widget should hide the drag handle and nudge.
  widget->Minimize();
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->ShowingNudge());
}

}  // namespace ash
