// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {
ShelfLayoutManager* GetShelfLayoutManager() {
  return AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
}
}  // namespace

class HotseatWidgetTest
    : public AshTestBase,
      public testing::WithParamInterface<ShelfAutoHideBehavior> {
 public:
  HotseatWidgetTest() = default;

  // Performs a swipe up gesture to show an auto-hidden shelf.
  void SwipeUpOnShelf() {
    gfx::Rect display_bounds =
        display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
    const gfx::Point start(display_bounds.bottom_center());
    const gfx::Point end(start + gfx::Vector2d(0, -80));
    const base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);
    const int kNumScrollSteps = 4;
    GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                               kNumScrollSteps);
  }

  void SwipeDownOnShelf() {
    gfx::Point start(GetPrimaryShelf()
                         ->shelf_widget()
                         ->shelf_view_for_testing()
                         ->GetBoundsInScreen()
                         .top_center());
    const gfx::Point end(start + gfx::Vector2d(0, 40));
    const base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);
    const int kNumScrollSteps = 4;
    GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                               kNumScrollSteps);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Used to test the Hotseat, ScrollabeShelf, and DenseShelf features.
INSTANTIATE_TEST_SUITE_P(All,
                         HotseatWidgetTest,
                         testing::Values(ShelfAutoHideBehavior::kNever,
                                         ShelfAutoHideBehavior::kAlways));

// Tests that closing a window which was opened prior to entering tablet mode
// results in a kShown hotseat.
TEST_P(HotseatWidgetTest, ClosingLastWindowInTabletMode) {
  GetPrimaryShelf()->SetAutoHideBehavior(GetParam());
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  // Activate the window and go to tablet mode.
  wm::ActivateWindow(window.get());
  TabletModeControllerTestApi().EnterTabletMode();

  // Close the window, the AppListView should be shown, and the hotseat should
  // be kShown.
  window->Hide();

  EXPECT_EQ(HotseatState::kShown, GetShelfLayoutManager()->hotseat_state());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the hotseat is kShown when entering tablet mode with no windows.
TEST_P(HotseatWidgetTest, GoingToTabletModeNoWindows) {
  GetPrimaryShelf()->SetAutoHideBehavior(GetParam());
  TabletModeControllerTestApi().EnterTabletMode();

  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(HotseatState::kShown, GetShelfLayoutManager()->hotseat_state());
}

// Tests that the hotseat is kHidden when entering tablet mode with a window.
TEST_P(HotseatWidgetTest, GoingToTabletModeWithWindows) {
  GetPrimaryShelf()->SetAutoHideBehavior(GetParam());

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  // Activate the window and go to tablet mode.
  wm::ActivateWindow(window.get());
  TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  GetAppListTestHelper()->CheckVisibility(false);
}

// The in-app Hotseat should not be hidden automatically when the shelf context
// menu shows (https://crbug.com/1020388).
TEST_P(HotseatWidgetTest, InAppShelfShowingContextMenu) {
  GetPrimaryShelf()->SetAutoHideBehavior(GetParam());
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());

  ShelfTestUtil::AddAppShortcut("app_id", TYPE_PINNED_APP);

  // Swipe up on the shelf to show the hotseat.
  SwipeUpOnShelf();
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  ShelfViewTestAPI shelf_view_test_api(
      GetPrimaryShelf()->shelf_widget()->shelf_view_for_testing());
  ShelfAppButton* app_icon = shelf_view_test_api.GetButton(0);

  // Accelerate the generation of the long press event.
  ui::GestureConfiguration::GetInstance()->set_show_press_delay_in_ms(1);
  ui::GestureConfiguration::GetInstance()->set_long_press_time_in_ms(1);

  // Press the icon enough long time to generate the long press event.
  GetEventGenerator()->MoveTouch(app_icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressTouch();
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  const int long_press_delay_ms = gesture_config->long_press_time_in_ms() +
                                  gesture_config->show_press_delay_in_ms();
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(long_press_delay_ms));
  run_loop.Run();
  GetEventGenerator()->ReleaseTouch();

  // Expects that the hotseat's state is kExntended.
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Ensures that the ink drop state is InkDropState::ACTIVATED before closing
  // the menu.
  app_icon->FireRippleActivationTimerForTest();
}

// Tests that a window that is created after going to tablet mode, then closed,
// results in a kShown hotseat.
TEST_P(HotseatWidgetTest, CloseLastWindowOpenedInTabletMode) {
  GetPrimaryShelf()->SetAutoHideBehavior(GetParam());
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  // Activate the window after entering tablet mode.
  wm::ActivateWindow(window.get());

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  GetAppListTestHelper()->CheckVisibility(false);

  // Hide the window, the hotseat should be kShown, and the home launcher should
  // be visible.
  window->Hide();

  EXPECT_EQ(HotseatState::kShown, GetShelfLayoutManager()->hotseat_state());
  GetAppListTestHelper()->CheckVisibility(true);
}

}  // namespace ash
