// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_launcher_gesture_handler.h"

#include "ash/home_screen/drag_window_from_shelf_controller.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/rounded_label_widget.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using Mode = HomeLauncherGestureHandler::Mode;

class HomeLauncherGestureHandlerTest : public AshTestBase {
 public:
  HomeLauncherGestureHandlerTest() = default;
  ~HomeLauncherGestureHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    // Wait for TabletModeController::Ctor to finish.
    base::RunLoop().RunUntilIdle();

    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  }

  // Create a test window and set the base transform to identity and
  // the base opacity to opaque for easier testing.
  virtual std::unique_ptr<aura::Window> CreateWindowForTesting() {
    std::unique_ptr<aura::Window> window = CreateTestWindow();
    window->SetTransform(gfx::Transform());
    window->layer()->SetOpacity(1.f);
    return window;
  }

  HomeLauncherGestureHandler* GetGestureHandler() {
    return Shell::Get()
        ->home_screen_controller()
        ->home_launcher_gesture_handler();
  }

  void DoPress(Mode mode) {
    DCHECK_NE(mode, Mode::kNone);
    gfx::Point press_location;
    if (mode == Mode::kSlideUpToShow) {
      press_location = Shelf::ForWindow(Shell::GetPrimaryRootWindow())
                           ->GetIdealBounds()
                           .CenterPoint();
    }

    GetGestureHandler()->OnPressEvent(mode, press_location);
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeLauncherGestureHandlerTest);
};

// Tests that the gesture handler will not have a window to act on if there are
// none in the mru list.
TEST_F(HomeLauncherGestureHandlerTest, NeedsOneWindowToShow) {
  DoPress(Mode::kSlideUpToShow);
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());

  auto window = CreateWindowForTesting();
  DoPress(Mode::kSlideUpToShow);
  EXPECT_TRUE(GetGestureHandler()->GetActiveWindow());
}

// Tests that the gesture handler will not have a window to act on if there are
// none in the mru list, or if they are not minimized.
TEST_F(HomeLauncherGestureHandlerTest, NeedsOneMinimizedWindowToHide) {
  DoPress(Mode::kSlideDownToHide);
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());

  auto window = CreateWindowForTesting();
  DoPress(Mode::kSlideDownToHide);
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());

  WindowState::Get(window.get())->Minimize();
  DoPress(Mode::kSlideDownToHide);
  EXPECT_TRUE(GetGestureHandler()->GetActiveWindow());
}

// Tests that if there are other visible windows behind the most recent one,
// they get hidden on press event so that the home launcher is visible.
TEST_F(HomeLauncherGestureHandlerTest, ShowWindowsAreHidden) {
  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window3 = CreateWindowForTesting();
  ASSERT_TRUE(window1->IsVisible());
  ASSERT_TRUE(window2->IsVisible());
  ASSERT_TRUE(window3->IsVisible());

  // Test that the most recently activated window is visible, but the others are
  // not.
  wm::ActivateWindow(window1.get());
  DoPress(Mode::kSlideUpToShow);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
}

TEST_F(HomeLauncherGestureHandlerTest, CancellingSlideUp) {
  UpdateDisplay("400x456");

  auto window = CreateWindowForTesting();
  ASSERT_TRUE(window->IsVisible());

  // Tests that when cancelling a scroll that was on the bottom half, the window
  // is still visible.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 0.f, 1.f);
  GetGestureHandler()->Cancel();
  EXPECT_TRUE(window->IsVisible());

  // Tests that when cancelling a scroll that was on the top half, the window is
  // now invisible.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 100), 0.f, 1.f);
  GetGestureHandler()->Cancel();
  EXPECT_FALSE(window->IsVisible());
}

// Tests that if we fling with enough velocity while sliding up, the launcher
// becomes visible even if the event is released below the halfway mark.
TEST_F(HomeLauncherGestureHandlerTest, FlingingSlideUp) {
  UpdateDisplay("400x456");

  auto window = CreateWindowForTesting();
  ASSERT_TRUE(window->IsVisible());

  // Tests that flinging down in this mode will keep the window visible.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 0.f, 10.f);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300),
                                      /*velocity_y=*/base::nullopt);
  ASSERT_TRUE(window->IsVisible());

  // Tests that flinging up in this mode will hide the window and show the
  // home launcher.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 0.f, -10.f);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_FALSE(window->IsVisible());
}

// Tests that if we fling with enough velocity while sliding up, the launcher
// becomes visible even if the event is released below the halfway mark.
TEST_F(HomeLauncherGestureHandlerTest, FlingingSlideDown) {
  UpdateDisplay("400x456");

  auto window = CreateWindowForTesting();
  WindowState::Get(window.get())->Minimize();
  ASSERT_FALSE(window->IsVisible());

  // Tests that flinging up in this mode will not show the mru window.
  DoPress(Mode::kSlideDownToHide);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 100), 0.f, -10.f);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100),
                                      /*velocity_y=*/base::nullopt);
  ASSERT_FALSE(window->IsVisible());

  // Tests that flinging down in this mode will show the mru window.
  DoPress(Mode::kSlideDownToHide);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 100), 0.f, 10.f);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(window->IsVisible());
}

TEST_F(HomeLauncherGestureHandlerTest, SlidingBelowPressPoint) {
  UpdateDisplay("400x456");

  auto window = CreateWindowForTesting();
  ASSERT_TRUE(window->IsVisible());

  // Tests that the windows transform does not change when trying to slide below
  // the press event location.
  GetGestureHandler()->OnPressEvent(Mode::kSlideUpToShow, gfx::Point(0, 400));
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 420), 0.f, 1.f);
  EXPECT_EQ(gfx::Transform(), window->transform());
}

// Tests that the home launcher gestures work with overview mode as expected.
TEST_F(HomeLauncherGestureHandlerTest, OverviewMode) {
  UpdateDisplay("400x456");

  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  EXPECT_FALSE(WindowState::Get(window1.get())->IsMinimized());
  EXPECT_FALSE(WindowState::Get(window2.get())->IsMinimized());

  OverviewController* controller = Shell::Get()->overview_controller();
  controller->StartOverview();
  const int window1_initial_translation =
      window1->transform().To2dTranslation().y();
  const int window2_initial_translation =
      window2->transform().To2dTranslation().y();
  DoPress(Mode::kSlideUpToShow);
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());

  // Tests that while scrolling the window transform changes.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 0.f, 1.f);
  EXPECT_NE(window1_initial_translation,
            window1->transform().To2dTranslation().y());
  EXPECT_NE(window2_initial_translation,
            window2->transform().To2dTranslation().y());

  // Tests that after releasing at below the halfway point, we remain in
  // overview mode.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(controller->InOverviewSession());
  EXPECT_EQ(window1_initial_translation,
            window1->transform().To2dTranslation().y());
  EXPECT_EQ(window2_initial_translation,
            window2->transform().To2dTranslation().y());

  // Tests that after releasing on the top half, overview mode has been
  // exited, and the two windows have been minimized to show the home launcher.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_FALSE(controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMinimized());
}

TEST_F(HomeLauncherGestureHandlerTest, OverviewModeNoWindows) {
  UpdateDisplay("400x456");

  OverviewController* controller = Shell::Get()->overview_controller();
  controller->StartOverview();
  views::Widget* no_windows_widget = static_cast<views::Widget*>(
      controller->overview_session()->no_windows_widget_for_testing());
  ASSERT_TRUE(no_windows_widget);
  aura::Window* widget_window = no_windows_widget->GetNativeWindow();

  DoPress(Mode::kSlideUpToShow);
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());

  // Tests that while scrolling the window transform changes.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 0.f, 1.f);
  EXPECT_EQ(0.f, widget_window->transform().To2dTranslation().x());
  EXPECT_NE(0.f, widget_window->transform().To2dTranslation().y());

  // Tests that after releasing at below the halfway point, we remain in
  // overview mode.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(controller->InOverviewSession());

  // Tests that after releasing on the top half, overview mode has been
  // exited.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_FALSE(controller->InOverviewSession());
}

// Tests that there is no crash if entering overview mode while home launcher is
// still in process of animating a window.
TEST_F(HomeLauncherGestureHandlerTest, OverviewModeEnteredWhileAnimating) {
  UpdateDisplay("400x456");
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto window = CreateWindowForTesting();
  GetGestureHandler()->ShowHomeLauncher(
      display_manager()->FindDisplayContainingPoint(gfx::Point(10, 10)));
  Shell::Get()->overview_controller()->StartOverview();
}

// Tests that HomeLauncherGestureHandler works as expected when one window is
// snapped, and overview mode is active on the other side.
TEST_F(HomeLauncherGestureHandlerTest, SplitviewOneSnappedWindow) {
  UpdateDisplay("400x456");

  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();

  // Snap one window and leave overview mode open with the other window.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(overview_controller->InOverviewSession());
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  const int window2_initial_translation =
      window2->transform().To2dTranslation().y();
  DoPress(Mode::kSlideUpToShow);
  EXPECT_EQ(window1.get(), GetGestureHandler()->GetActiveWindow());

  // Tests that while scrolling the window transforms change.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 0.f, 1.f);
  EXPECT_NE(window1->transform(), gfx::Transform());
  EXPECT_NE(window2_initial_translation,
            window2->transform().To2dTranslation().y());

  // Tests that after releasing at below the halfway point, we remain in
  // both splitview and overview mode.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_EQ(window1->transform(), gfx::Transform());
  EXPECT_EQ(window2_initial_translation,
            window2->transform().To2dTranslation().y());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Tests that after releasing on the top half, overivew and splitview have
  // both been exited, and both windows are minimized to show the home launcher.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMinimized());
}

// Tests that swipe to close works as expected when there are two snapped
// windows.
TEST_F(HomeLauncherGestureHandlerTest, SplitviewTwoSnappedWindows) {
  UpdateDisplay("400x456");

  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();

  // Snap two windows to start.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Make |window1| the most recent used window. It should be the main window in
  // HomeLauncherGestureHandler.
  wm::ActivateWindow(window1.get());
  DoPress(Mode::kSlideUpToShow);
  EXPECT_EQ(window1.get(), GetGestureHandler()->GetActiveWindow());
  EXPECT_EQ(window2.get(), GetGestureHandler()->GetSecondaryWindow());

  // Tests that while scrolling the window transforms change.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 0.f, 1.f);
  EXPECT_NE(window1->transform(), gfx::Transform());
  EXPECT_NE(window2->transform(), gfx::Transform());

  // Tests that after releasing at below the halfway point, we remain in
  // splitview.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_EQ(window1->transform(), gfx::Transform());
  EXPECT_EQ(window2->transform(), gfx::Transform());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Tests that after releasing on the bottom half, splitview has been ended,
  // and the two windows have been minimized to show the home launcher.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMinimized());
}

// Tests that the shelf background is transparent when the home launcher is
// dragged, and does not shift to opaque until the home launcher is completely
// hidden from view and the finger is released.
TEST_F(HomeLauncherGestureHandlerTest, TransparentShelfWileDragging) {
  UpdateDisplay("400x456");

  auto window = CreateWindowForTesting();
  ASSERT_TRUE(window->IsVisible());
  ASSERT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_MAXIMIZED,
            AshTestBase::GetPrimaryShelf()
                ->shelf_layout_manager()
                ->GetShelfBackgroundType());

  // Begin to show the home launcher, the shelf should become transparent.
  DoPress(Mode::kSlideUpToShow);
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT,
            AshTestBase::GetPrimaryShelf()
                ->shelf_layout_manager()
                ->GetShelfBackgroundType());

  // Fling up to complete showing the home launcher, the shelf should remain
  // transparent.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 0.f, -10.f);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT,
            AshTestBase::GetPrimaryShelf()
                ->shelf_layout_manager()
                ->GetShelfBackgroundType());

  // Begin to hide the home launcher, the background should still be
  // transparent.
  DoPress(Mode::kSlideDownToHide);
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT,
            AshTestBase::GetPrimaryShelf()
                ->shelf_layout_manager()
                ->GetShelfBackgroundType());

  // Fling down to hide the home launcher, the shelf should still be
  // transparent.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 100), 0.f, -10.f);
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT,
            AshTestBase::GetPrimaryShelf()
                ->shelf_layout_manager()
                ->GetShelfBackgroundType());

  // The shelf should transition to opauqe when the gesture sequence has
  // completed.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_MAXIMIZED,
            AshTestBase::GetPrimaryShelf()
                ->shelf_layout_manager()
                ->GetShelfBackgroundType());
}

// Tests that in kDragWindowToHomeOrOverview mode, the dragged active window
// might be different in different scenarios.
TEST_F(HomeLauncherGestureHandlerTest, DraggedActiveWindow) {
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();

  // We need at least one window to work with.
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());

  auto window1 = CreateWindowForTesting();
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  EXPECT_EQ(GetGestureHandler()->GetActiveWindow(), window1.get());
  GetGestureHandler()->OnReleaseEvent(shelf_bounds.CenterPoint(),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());

  // Test in splitview, depends on the drag position, the active dragged window
  // might be different.
  auto window2 = CreateWindowForTesting();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.bottom_left());
  EXPECT_EQ(GetGestureHandler()->GetActiveWindow(), window1.get());
  GetGestureHandler()->OnReleaseEvent(shelf_bounds.bottom_left(),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.bottom_right());
  EXPECT_EQ(GetGestureHandler()->GetActiveWindow(), window2.get());
  GetGestureHandler()->OnReleaseEvent(shelf_bounds.bottom_right(),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());
  split_view_controller()->EndSplitView();

  // In overview, drag from shelf is a no-op.
  Shell::Get()->overview_controller()->StartOverview();
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());
}

// Tests that in kDragWindowToHomeOrOverview mode, we may hide different sets
// of windows in different scenarios.
TEST_F(HomeLauncherGestureHandlerTest, HideWindowDuringWindowDragging) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();

  auto window3 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window1 = CreateWindowForTesting();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(200, 200), 1.f, 1.f);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  GetGestureHandler()->OnReleaseEvent(shelf_bounds.CenterPoint(),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  // In splitview mode, the snapped windows will stay visible during dragging.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.bottom_left());
  EXPECT_EQ(GetGestureHandler()->GetActiveWindow(), window1.get());
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 200), 1.f, 1.f);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  GetGestureHandler()->OnReleaseEvent(shelf_bounds.bottom_left(),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.bottom_right());
  GetGestureHandler()->OnScrollEvent(gfx::Point(400, 200), 1.f, 1.f);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  GetGestureHandler()->OnReleaseEvent(shelf_bounds.bottom_right(),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
}

// Test in kDragWindowToHomeOrOverview mode, home launcher is hidden during
// dragging.
TEST_F(HomeLauncherGestureHandlerTest, HideHomeLauncherDuringDraggingTest) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateWindowForTesting();
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 0.f, 1.f);
  aura::Window* home_screen_window =
      Shell::Get()->home_screen_controller()->delegate()->GetHomeScreenWindow();
  EXPECT_TRUE(home_screen_window);
  EXPECT_FALSE(home_screen_window->IsVisible());

  GetGestureHandler()->OnReleaseEvent(shelf_bounds.CenterPoint(),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(home_screen_window->IsVisible());
}

// Test in kDragWindowToHomeOrOverview mode, drag from shelf when overview is
// active is a no-op.
TEST_F(HomeLauncherGestureHandlerTest, NoOpInOverview) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_FALSE(GetGestureHandler()->OnPressEvent(
      Mode::kDragWindowToHomeOrOverview, shelf_bounds.CenterPoint()));
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());
  overview_controller->EndOverview();

  // In splitview + overview case, drag from shelf in the overview side of the
  // screen also does nothing.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  overview_controller->StartOverview();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(GetGestureHandler()->OnPressEvent(
      Mode::kDragWindowToHomeOrOverview, shelf_bounds.bottom_right()));
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());
}

// Test in kDragWindowToHomeOrOverview mode, the windows that were hidden before
// drag started may or may not reshow, depending on different scenarios.
TEST_F(HomeLauncherGestureHandlerTest, MayOrMayNotReShowHiddenWindows) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window2 = CreateWindowForTesting();
  auto window1 = CreateWindowForTesting();

  // If the dragged window restores to its original position, reshow the hidden
  // windows.
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_FALSE(window2->IsVisible());
  GetGestureHandler()->OnReleaseEvent(shelf_bounds.CenterPoint(),
                                      base::nullopt);
  EXPECT_TRUE(window2->IsVisible());

  // If fling to homescreen, do not reshow the hidden windows.
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_FALSE(window2->IsVisible());
  GetGestureHandler()->OnReleaseEvent(
      gfx::Point(200, 200),
      -DragWindowFromShelfController::kVelocityToHomeScreenThreshold);
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());

  // If the dragged window is added to overview, do not reshow the hidden
  // windows.
  window2->Show();
  window1->Show();
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_FALSE(window2->IsVisible());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(gfx::Point(200, 200), base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_FALSE(window2->IsVisible());
  overview_controller->EndOverview();

  // If the dragged window is snapped in splitview, while the other windows are
  // showing in overview, do not reshow the hidden windows.
  window2->Show();
  window1->Show();
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 200), 0.f, 1.f);
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 200), base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_FALSE(window2->IsVisible());
}

// Test that in kDragWindowToHomeOrOverview mode, during window dragging, if
// overview is open, the minimized windows can show correctly in overview.
TEST_F(HomeLauncherGestureHandlerTest, MinimizedWindowsShowInOverview) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window3 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window1 = CreateWindowForTesting();

  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
  // Drag it far enough so overview should be open behind the dragged window.
  GetGestureHandler()->OnScrollEvent(gfx::Point(200, 200), 0.f, 1.f);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMinimized());
  EXPECT_FALSE(window3->IsVisible());
  EXPECT_TRUE(WindowState::Get(window3.get())->IsMinimized());
  EXPECT_FALSE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window2.get()));
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window3.get()));
  // Release the drag, the window should be added to overview.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(200, 200), base::nullopt);
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
}

// Test that in kDragWindowToHomeOrOverview mode, when swiping up from the
// shelf, we only open overview when the y scroll delta (velocity) decrease to
// kOpenOverviewThreshold or less.
TEST_F(HomeLauncherGestureHandlerTest, OpenOverviewWhenHold) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateWindowForTesting();

  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(
      gfx::Point(200, 200), 0.f,
      DragWindowFromShelfController::kOpenOverviewThreshold + 1);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnScrollEvent(
      gfx::Point(200, 200), 0.f,
      DragWindowFromShelfController::kOpenOverviewThreshold);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(gfx::Point(200, 200), base::nullopt);
}

// Test that in kDragWindowToHomeOrOverview mode, if the dragged window is not
// dragged far enough than kReturnToMaximizedThreshold, it will restore back to
// its original position.
TEST_F(HomeLauncherGestureHandlerTest, RestoreWindowToOriginalBounds) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateWindowForTesting();
  const gfx::Rect work_area = display::Screen::GetScreen()
                                  ->GetDisplayNearestWindow(window.get())
                                  .work_area();

  // Drag it for a small distance and then release.
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(
      gfx::Point(200, 300), 0.f,
      DragWindowFromShelfController::kShowOverviewThreshold + 1);
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(gfx::Point(200, 400), base::nullopt);
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // Drag it for a large distance and then drag back to release.
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(
      gfx::Point(
          200, work_area.bottom() -
                   DragWindowFromShelfController::kReturnToMaximizedThreshold +
                   1),
      base::nullopt);
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // The same thing should happen if splitview mode is active.
  auto window2 = CreateWindowForTesting();
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.bottom_left());
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 200), 0.f, 1.f);
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 400), base::nullopt);
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(split_view_controller()->left_window(), window.get());
}

// Test that in kDragWindowToHomeOrOverview mode, if overview is active and
// splitview is not active, fling in overview may or may not head to the home
// screen.
TEST_F(HomeLauncherGestureHandlerTest, FlingInOverview) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateWindowForTesting();

  // If fling velocity is smaller than kVelocityToHomeScreenThreshold, decide
  // where the window should go based on the release position.
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(200, 200), 0.f, 1.f);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(
      gfx::Point(0, 350),
      base::make_optional(
          -DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));
  // The window should restore back to its original position.
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // If fling velocity is equal or larger than kVelocityToHomeScreenThreshold
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(
      gfx::Point(0, 350),
      base::make_optional(
          -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
}

// Test that in kDragWindowToHomeOrOverview mode, if splitview is active when
// fling happens, the window will be put in overview.
TEST_F(HomeLauncherGestureHandlerTest, DragOrFlingInSplitView) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();

  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // If the window is only dragged for a small distance:
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.bottom_left());
  GetGestureHandler()->OnScrollEvent(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(gfx::Point(100, 350), base::nullopt);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));

  // If the window is dragged for a long distance:
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.bottom_left());
  GetGestureHandler()->OnScrollEvent(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(gfx::Point(100, 200), base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));
  overview_controller->EndOverview();

  // If the window is flung with a small velocity:
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.bottom_left());
  GetGestureHandler()->OnScrollEvent(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(
      gfx::Point(100, 350),
      base::make_optional(
          -DragWindowFromShelfController::kVelocityToOverviewThreshold + 10));
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));

  // If the window is flung with a large velocity:
  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.bottom_left());
  GetGestureHandler()->OnScrollEvent(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetGestureHandler()->OnReleaseEvent(
      gfx::Point(100, 350),
      base::make_optional(
          -DragWindowFromShelfController::kVelocityToOverviewThreshold));
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));
  overview_controller->EndOverview();
}

// Test that in kDragWindowToHomeOrOverview mode, wallpaper should be blurred
// as in overview, even though overview might not open during dragging.
TEST_F(HomeLauncherGestureHandlerTest, WallpaperBlurDuringDragging) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateWindowForTesting();

  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(
      gfx::Point(shelf_bounds.x(), shelf_bounds.y() - 1), 0.f,
      DragWindowFromShelfController::kShowOverviewThreshold + 1);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  auto* wallpaper_view =
      RootWindowController::ForWindow(window->GetRootWindow())
          ->wallpaper_widget_controller()
          ->wallpaper_view();
  EXPECT_EQ(wallpaper_view->repaint_blur(), kWallpaperBlurSigma);

  GetGestureHandler()->OnReleaseEvent(shelf_bounds.CenterPoint(),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_EQ(wallpaper_view->repaint_blur(), kWallpaperClearBlurSigma);
}

// Test that in kDragWindowToHomeOrOverview mode, overview is hidden during
// dragging and shown when drag slows down or stops.
TEST_F(HomeLauncherGestureHandlerTest, HideOverviewDuringDragging) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window2 = CreateWindowForTesting();
  auto window1 = CreateWindowForTesting();

  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(200, 200), 0.5f, 0.5f);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  // We test the visibility of overview by testing the drop target widget's
  // visibility in the overview.
  OverviewGrid* current_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          window1->GetRootWindow());
  OverviewItem* drop_target_item = current_grid->GetDropTarget();
  EXPECT_TRUE(drop_target_item);
  EXPECT_EQ(drop_target_item->GetWindow()->layer()->GetTargetOpacity(), 1.f);

  GetGestureHandler()->OnScrollEvent(
      gfx::Point(200, 200), 0.5f,
      DragWindowFromShelfController::kShowOverviewThreshold + 1);
  // Test overview should be invisble.
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(drop_target_item->GetWindow()->layer()->GetTargetOpacity(), 0.f);

  GetGestureHandler()->OnReleaseEvent(gfx::Point(200, 200),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  // |window1| should have added to overview. Test its visibility.
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_EQ(window1->layer()->GetTargetOpacity(), 1.f);
}

// Test that in kDragWindowToHomeOrOverview mode, we do not show drag-to-snap
// or cannot-snap drag indicators.
TEST_F(HomeLauncherGestureHandlerTest, NoDragToSnapIndicator) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateWindowForTesting();

  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(200, 200), 0.5f, 0.5f);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  OverviewSession* overview_session = overview_controller->overview_session();
  IndicatorState indicator_state =
      overview_session->split_view_drag_indicators()->current_indicator_state();
  EXPECT_EQ(indicator_state, IndicatorState::kNone);

  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 200), 0.5f, 0.5f);
  indicator_state =
      overview_session->split_view_drag_indicators()->current_indicator_state();
  EXPECT_EQ(indicator_state, IndicatorState::kPreviewAreaLeft);

  GetGestureHandler()->OnReleaseEvent(shelf_bounds.CenterPoint(),
                                      /*velocity_y=*/base::nullopt);
}

// Test there is no black backdrop behind the dragged window if we're doing the
// scale down animation for the dragged window.
TEST_F(HomeLauncherGestureHandlerTest, NoBackdropDuringWindowScaleDown) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateWindowForTesting();
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_NE(window->GetProperty(kBackdropWindowMode),
            BackdropWindowMode::kDisabled);

  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 200), 0.f, 10.f);
  GetGestureHandler()->OnReleaseEvent(
      shelf_bounds.CenterPoint(),
      base::make_optional(
          -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_EQ(window->GetProperty(kBackdropWindowMode),
            BackdropWindowMode::kDisabled);
}

// Test that if drag is cancelled, overview should be dismissed and other
// hidden windows should restore to its previous visibility state.
TEST_F(HomeLauncherGestureHandlerTest, CancelDragDismissOverview) {
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window3 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window1 = CreateWindowForTesting();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  GetGestureHandler()->OnPressEvent(Mode::kDragWindowToHomeOrOverview,
                                    shelf_bounds.CenterPoint());
  GetGestureHandler()->OnScrollEvent(gfx::Point(200, 200), 0.5f, 0.5f);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());

  GetGestureHandler()->Cancel();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
}

class HomeLauncherModeGestureHandlerTest
    : public HomeLauncherGestureHandlerTest,
      public testing::WithParamInterface<Mode> {
 public:
  HomeLauncherModeGestureHandlerTest() : mode_(GetParam()) {}
  virtual ~HomeLauncherModeGestureHandlerTest() = default;

  // HomeLauncherGestureHandlerTest:
  std::unique_ptr<aura::Window> CreateWindowForTesting() override {
    std::unique_ptr<aura::Window> window =
        HomeLauncherGestureHandlerTest::CreateWindowForTesting();
    if (mode_ == Mode::kSlideDownToHide)
      WindowState::Get(window.get())->Minimize();
    return window;
  }

 protected:
  Mode mode_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeLauncherModeGestureHandlerTest);
};

INSTANTIATE_TEST_SUITE_P(,
                         HomeLauncherModeGestureHandlerTest,
                         testing::Values(Mode::kSlideDownToHide,
                                         Mode::kSlideUpToShow));

// Tests that the window transform and opacity changes as we scroll.
TEST_P(HomeLauncherModeGestureHandlerTest, TransformAndOpacityChangesOnScroll) {
  auto window = CreateWindowForTesting();

  DoPress(mode_);
  ASSERT_TRUE(GetGestureHandler()->GetActiveWindow());

  // Test that on scrolling to a point on the top half of the work area, the
  // window's opacity is between 0 and 0.5 and its transform has changed.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 100), 0.f, 1.f);
  const gfx::Transform top_half_transform = window->transform();
  EXPECT_NE(gfx::Transform(), top_half_transform);
  EXPECT_GT(window->layer()->opacity(), 0.f);
  EXPECT_LT(window->layer()->opacity(), 0.5f);

  // Test that on scrolling to a point on the bottom half of the work area, the
  // window's opacity is between 0.5 and 1 and its transform has changed.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 0.f, 1.f);
  EXPECT_NE(gfx::Transform(), window->transform());
  EXPECT_NE(gfx::Transform(), top_half_transform);
  EXPECT_GT(window->layer()->opacity(), 0.5f);
  EXPECT_LT(window->layer()->opacity(), 1.f);
}

// Tests that releasing a drag at the bottom of the work area will show the
// window.
TEST_P(HomeLauncherModeGestureHandlerTest, BelowHalfShowsWindow) {
  UpdateDisplay("400x400");
  auto window3 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window1 = CreateWindowForTesting();

  DoPress(mode_);
  ASSERT_TRUE(GetGestureHandler()->GetActiveWindow());
  ASSERT_FALSE(window2->IsVisible());
  ASSERT_FALSE(window3->IsVisible());

  // After a scroll the transform and opacity are no longer the identity and 1.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 0.f, 1.f);
  EXPECT_NE(gfx::Transform(), window1->transform());
  EXPECT_NE(1.f, window1->layer()->opacity());

  // Tests the transform and opacity have returned to the identity and 1.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_EQ(gfx::Transform(), window1->transform());
  EXPECT_EQ(1.f, window1->layer()->opacity());

  if (mode_ == Mode::kSlideDownToHide)
    return;

  // The other windows return to their original visibility if mode is swiping
  // up.
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
}

// Tests that a drag released at the top half of the work area will minimize the
// window under action.
TEST_P(HomeLauncherModeGestureHandlerTest, AboveHalfReleaseMinimizesWindow) {
  UpdateDisplay("400x400");
  auto window3 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window1 = CreateWindowForTesting();

  DoPress(mode_);
  ASSERT_TRUE(GetGestureHandler()->GetActiveWindow());
  ASSERT_FALSE(window2->IsVisible());
  ASSERT_FALSE(window3->IsVisible());

  // Test that |window1| is minimized on release.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMinimized());

  // The rest of the windows remain invisible, to show the home launcher.
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
}

// Tests on swipe up, the transient child of a window which is getting hidden
// will have its opacity and transform altered as well.
TEST_P(HomeLauncherModeGestureHandlerTest, WindowWithTransientChild) {
  UpdateDisplay("400x456");

  // Create a window with a transient child.
  auto parent = CreateWindowForTesting();
  auto child = CreateTestWindow(gfx::Rect(100, 100, 200, 200),
                                aura::client::WINDOW_TYPE_POPUP);
  child->SetTransform(gfx::Transform());
  child->layer()->SetOpacity(1.f);
  ::wm::AddTransientChild(parent.get(), child.get());

  // |parent| should be the window that is getting hidden.
  DoPress(mode_);
  ASSERT_EQ(parent.get(), GetGestureHandler()->GetActiveWindow());

  // Tests that after scrolling to the halfway point, the transient child's
  // opacity and transform are halfway to their final values.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 200), 0.f, 1.f);
  EXPECT_LE(0.45f, child->layer()->opacity());
  EXPECT_GE(0.55f, child->layer()->opacity());
  EXPECT_NE(gfx::Transform(), child->transform());

  // Tests that after releasing on the bottom half, the transient child reverts
  // to its original values.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_EQ(1.0f, child->layer()->opacity());
  EXPECT_EQ(gfx::Transform(), child->transform());
}

// Tests that when tablet mode is ended while in the middle of a scroll session,
// the window is advanced to its end state.
TEST_P(HomeLauncherModeGestureHandlerTest, EndScrollOnTabletModeEnd) {
  auto window = CreateWindowForTesting();

  DoPress(mode_);
  ASSERT_TRUE(GetGestureHandler()->GetActiveWindow());

  // Scroll to a point above the halfway mark of the work area.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 50), 0.f, 1.f);
  EXPECT_TRUE(GetGestureHandler()->GetActiveWindow());
  EXPECT_FALSE(WindowState::Get(window.get())->IsMinimized());

  // Tests that on exiting tablet mode, |window| gets minimized and is no longer
  // tracked by the gesture handler.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
}

// Tests that the variables get set as expected during dragging, and get reset
// after finishing a drag.
TEST_P(HomeLauncherModeGestureHandlerTest, AnimatingToEndResetsState) {
  // Create a window with a transient child to test that case.
  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto child = CreateTestWindow(gfx::Rect(100, 100, 200, 200),
                                aura::client::WINDOW_TYPE_POPUP);
  ::wm::AddTransientChild(window1.get(), child.get());
  wm::ActivateWindow(window1.get());

  // For swipe down to hide launcher, all windows must be minimized.
  if (mode_ == Mode::kSlideDownToHide) {
    WindowState::Get(window2.get())->Minimize();
    WindowState::Get(window1.get())->Minimize();
  }

  // Tests that the variables which change when dragging are as expected.
  DoPress(mode_);
  EXPECT_EQ(window1.get(), GetGestureHandler()->GetActiveWindow());
  EXPECT_TRUE(GetGestureHandler()->last_event_location_);
  EXPECT_EQ(mode_, GetGestureHandler()->mode_);
  // We only need to hide windows when swiping up, so this will only be non
  // empty in that case.
  if (mode_ == Mode::kSlideUpToShow)
    EXPECT_FALSE(GetGestureHandler()->hidden_windows_.empty());

  // Tests that after a drag, the variables are either null or empty.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(10, 10),
                                      /*velocity_y=*/base::nullopt);
  EXPECT_FALSE(GetGestureHandler()->GetActiveWindow());
  EXPECT_FALSE(GetGestureHandler()->last_event_location_);
  EXPECT_EQ(Mode::kNone, GetGestureHandler()->mode_);
  EXPECT_TRUE(GetGestureHandler()->hidden_windows_.empty());
}

}  // namespace ash
