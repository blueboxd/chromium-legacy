// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_screen_controller.h"

#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"

namespace ash {
namespace {

constexpr char kHomescreenAnimationHistogram[] =
    "Ash.Homescreen.AnimationSmoothness";
constexpr char kHomescreenDragHistogram[] =
    "Apps.StateTransition.Drag.PresentationTime.TabletMode";
constexpr char kHomescreenDragMaxLatencyHistogram[] =
    "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode";

// Parameterized depending on whether navigation gestures for swiping from shelf
// to home/overview are enabled.
class HomeScreenControllerTest : public AshTestBase,
                                 public testing::WithParamInterface<bool> {
 public:
  HomeScreenControllerTest() {
    if (IsWindowDragFromShelfEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kDragFromShelfToHomeOrOverview}, {});
    } else {
      // The feature verified by this test is only enabled if drag from shelf to
      // home or overview is disabled.
      scoped_feature_list_.InitWithFeatures(
          {}, {features::kDragFromShelfToHomeOrOverview,
               chromeos::features::kShelfHotseat});
    }
  }
  ~HomeScreenControllerTest() override = default;

  std::unique_ptr<aura::Window> CreateTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  }

  std::unique_ptr<aura::Window> CreatePopupTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400),
                                         aura::client::WINDOW_TYPE_POPUP);
  }

  bool IsWindowDragFromShelfEnabled() const { return GetParam(); }

  HomeScreenController* home_screen_controller() {
    return Shell::Get()->home_screen_controller();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeScreenControllerTest);
};

INSTANTIATE_TEST_SUITE_P(All, HomeScreenControllerTest, testing::Bool());

TEST_P(HomeScreenControllerTest, OnlyMinimizeCycleListWindows) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreatePopupTestWindow());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<ui::Event> test_event = std::make_unique<ui::KeyEvent>(
      ui::EventType::ET_MOUSE_PRESSED, ui::VKEY_UNKNOWN, ui::EF_NONE);
  home_screen_controller()->GoHome(GetPrimaryDisplay().id());
  ASSERT_TRUE(WindowState::Get(w1.get())->IsMinimized());
  ASSERT_FALSE(WindowState::Get(w2.get())->IsMinimized());
}

// Tests that the home screen is visible after rotating the screen in overview
// mode.
TEST_P(HomeScreenControllerTest,
       HomeScreenVisibleAfterDisplayUpdateInOverview) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();

  // Trigger a display configuration change, this simulates screen rotation.
  Shell::Get()->app_list_controller()->OnDisplayConfigurationChanged();

  // End overview mode, the home launcher should be visible.
  overview_controller->EndOverview();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kExitAnimationComplete);

  EXPECT_TRUE(
      home_screen_controller()->delegate()->GetHomeScreenWindow()->IsVisible());
}

TEST_P(HomeScreenControllerTest, ShowLauncherHistograms) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  auto window = CreateTestWindow();
  base::HistogramTester tester;
  tester.ExpectTotalCount(kHomescreenAnimationHistogram, 0);

  // Note that going home animation minimizes the window, and the window layer
  // is recreated after minimization animation is set up. Create waiter before
  // the request to go home to ensure the waiter waits for the correct animator.
  base::OnceClosure waiter =
      ShellTestApi().CreateWaiterForFinishingWindowAnimation(window.get());

  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, 0);
  GetEventGenerator()->ReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, 0);

  std::move(waiter).Run();

  // If window drag from shelf is disabled, the active window is minimized using
  // different animation, and in that case the window might still be animating.
  if (!IsWindowDragFromShelfEnabled()) {
    ASSERT_TRUE(window->layer()->GetAnimator()->is_animating());
    ShellTestApi().WaitForWindowFinishAnimating(window.get());
  } else {
    ASSERT_FALSE(window->layer()->GetAnimator()->is_animating());
  }

  tester.ExpectTotalCount(kHomescreenAnimationHistogram, 1);
}

TEST_P(HomeScreenControllerTest, DraggingHistograms) {
  UpdateDisplay("400x400");

  // Create a window and then minimize it so we can drag from top to show
  // launcher.
  auto window = CreateTestWindow();
  WindowState::Get(window.get())->Minimize();

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  base::HistogramTester tester;
  tester.ExpectTotalCount(kHomescreenAnimationHistogram, 0);
  tester.ExpectTotalCount(kHomescreenDragHistogram, 0);
  tester.ExpectTotalCount(kHomescreenDragMaxLatencyHistogram, 0);

  const bool drag_enabled = !IsWindowDragFromShelfEnabled();

  // Create a touch event and drag it twice and verify the histograms are
  // recorded as expected.
  auto* compositor = GetContext()->layer()->GetCompositor();
  auto* generator = GetEventGenerator();
  generator->set_current_screen_location(gfx::Point(200, 1));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(200, 20));
  compositor->ScheduleFullRedraw();
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
  tester.ExpectTotalCount(kHomescreenDragHistogram, drag_enabled ? 1 : 0);
  generator->MoveTouch(gfx::Point(200, 60));
  compositor->ScheduleFullRedraw();
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
  generator->ReleaseTouch();

  tester.ExpectTotalCount(kHomescreenAnimationHistogram, 0);
  tester.ExpectTotalCount(kHomescreenDragHistogram, drag_enabled ? 2 : 0);
  tester.ExpectTotalCount(kHomescreenDragMaxLatencyHistogram,
                          drag_enabled ? 1 : 0);

  // On touch release, the window should animate. When it's done animating we
  // should have a animation smoothness histogram recorded.
  if (drag_enabled) {
    ShellTestApi().WaitForWindowFinishAnimating(window.get());
    tester.ExpectTotalCount(kHomescreenAnimationHistogram, 1);
  }
}

}  // namespace
}  // namespace ash
