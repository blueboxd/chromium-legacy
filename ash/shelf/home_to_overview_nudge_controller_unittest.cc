// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_to_overview_nudge_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/shelf/contextual_nudge.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class WidgetCloseObserver : public views::WidgetObserver {
 public:
  WidgetCloseObserver(views::Widget* widget) : widget_(widget) {
    if (widget_)
      widget_->AddObserver(this);
  }

  ~WidgetCloseObserver() override { CleanupWidget(); }

  bool WidgetClosed() const { return !widget_; }

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override { CleanupWidget(); }

  void CleanupWidget() {
    if (widget_) {
      widget_->RemoveObserver(this);
      widget_ = nullptr;
    }
  }

 private:
  views::Widget* widget_;
};

class HomeToOverviewNudgeControllerWithNudgesDisabledTest : public AshTestBase {
 public:
  HomeToOverviewNudgeControllerWithNudgesDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kContextualNudges);
  }
  ~HomeToOverviewNudgeControllerWithNudgesDisabledTest() override = default;

  HomeToOverviewNudgeControllerWithNudgesDisabledTest(
      const HomeToOverviewNudgeControllerWithNudgesDisabledTest& other) =
      delete;
  HomeToOverviewNudgeControllerWithNudgesDisabledTest& operator=(
      const HomeToOverviewNudgeControllerWithNudgesDisabledTest& other) =
      delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class HomeToOverviewNudgeControllerTest : public AshTestBase {
 public:
  HomeToOverviewNudgeControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kContextualNudges);
  }
  ~HomeToOverviewNudgeControllerTest() override = default;

  HomeToOverviewNudgeControllerTest(
      const HomeToOverviewNudgeControllerTest& other) = delete;
  HomeToOverviewNudgeControllerTest& operator=(
      const HomeToOverviewNudgeControllerTest& other) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    test_clock_.Advance(base::TimeDelta::FromHours(2));
    contextual_tooltip::OverrideClockForTesting(&test_clock_);
  }
  void TearDown() override {
    contextual_tooltip::ClearClockOverrideForTesting();
    AshTestBase::TearDown();
  }

  HomeToOverviewNudgeController* GetNudgeController() {
    return GetPrimaryShelf()
        ->shelf_layout_manager()
        ->home_to_overview_nudge_controller_for_testing();
  }

  views::Widget* GetNudgeWidget() {
    if (!GetNudgeController()->nudge_for_testing())
      return nullptr;
    return GetNudgeController()->nudge_for_testing()->GetWidget();
  }

  HotseatWidget* GetHotseatWidget() {
    return GetPrimaryShelf()->shelf_widget()->hotseat_widget();
  }

  void SanityCheckNudgeBounds() {
    views::Widget* const nudge_widget = GetNudgeWidget();
    ASSERT_TRUE(nudge_widget);
    EXPECT_TRUE(nudge_widget->IsVisible());

    gfx::RectF nudge_bounds_f(
        nudge_widget->GetNativeWindow()->GetTargetBounds());
    nudge_widget->GetLayer()->transform().TransformRect(&nudge_bounds_f);
    const gfx::Rect nudge_bounds = gfx::ToEnclosingRect(nudge_bounds_f);

    HotseatWidget* const hotseat = GetHotseatWidget();
    gfx::RectF hotseat_bounds_f(hotseat->GetNativeWindow()->GetTargetBounds());
    hotseat->GetLayer()->transform().TransformRect(&hotseat_bounds_f);
    const gfx::Rect hotseat_bounds = gfx::ToEnclosingRect(hotseat_bounds_f);

    // Nudge and hotseat should have the same transform.
    EXPECT_EQ(hotseat->GetLayer()->transform(),
              nudge_widget->GetLayer()->transform());

    // Nudge should be under the hotseat.
    EXPECT_LE(hotseat_bounds.bottom(), nudge_bounds.y());

    const gfx::Rect display_bounds = GetPrimaryDisplay().bounds();
    EXPECT_TRUE(display_bounds.Contains(nudge_bounds))
        << display_bounds.ToString() << " contains " << nudge_bounds.ToString();

    // Verify that the nudge is centered within the display bounds.
    EXPECT_LE((nudge_bounds.x() - display_bounds.x()) -
                  (display_bounds.right() - nudge_bounds.right()),
              1)
        << nudge_bounds.ToString() << " within " << display_bounds.ToString();

    // Verify that the nudge label is visible
    EXPECT_EQ(
        1.0f,
        GetNudgeController()->nudge_for_testing()->label()->layer()->opacity());
  }

  base::SimpleTestClock test_clock_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that home to overview gesture nudge is not shown if contextual nudges
// are disabled.
TEST_F(HomeToOverviewNudgeControllerWithNudgesDisabledTest,
       NoNudgeOnHomeScreen) {
  EXPECT_FALSE(GetPrimaryShelf()
                   ->shelf_layout_manager()
                   ->home_to_overview_nudge_controller_for_testing());
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(GetPrimaryShelf()
                   ->shelf_layout_manager()
                   ->home_to_overview_nudge_controller_for_testing());
}

// Test the flow for showing the home to overview gesture nudge - when shown the
// first time, nudge should remain visible until the hotseat state changes. On
// subsequent shows, the nudge should be hidden after a timeout.
TEST_F(HomeToOverviewNudgeControllerTest, ShownOnHomeScreen) {
  // The nudge should not be shown in clamshell.
  EXPECT_FALSE(GetNudgeController());

  // Entering tablt mode should schedule the nudge to get shown.
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(GetNudgeController());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();

  ASSERT_TRUE(GetNudgeController()->nudge_for_testing());
  {
    SCOPED_TRACE("First nudge");
    SanityCheckNudgeBounds();
  }
  EXPECT_FALSE(GetNudgeController()->HasHideTimerForTesting());

  // Transitioning to overview should hide the nudge.
  Shell::Get()->overview_controller()->StartOverview();

  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

  // Ending overview, and transitioning to the home screen again should not show
  // the nudge.
  Shell::Get()->overview_controller()->EndOverview();
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(gfx::Transform(), GetHotseatWidget()->GetLayer()->transform());

  // Advance time for more than a day (which should enable the nudge again).
  test_clock_.Advance(base::TimeDelta::FromHours(25));

  // The nudge should not show up unless the user actually transitions to home.
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

  // Create and delete a test window to force a transition to home.
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  WindowState::Get(window.get())->Minimize();

  // Nudge should be shown again.
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();
  ASSERT_TRUE(GetNudgeController()->nudge_for_testing());
  {
    SCOPED_TRACE("Second nudge");
    SanityCheckNudgeBounds();
  }

  // The second time, the nudge should be hidden after a timeout.
  ASSERT_TRUE(GetNudgeController()->HasHideTimerForTesting());
  GetNudgeController()->FireHideTimerForTesting();
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(gfx::Transform(), GetHotseatWidget()->GetLayer()->transform());
}

// Tests that the nudge eventually stops showing.
TEST_F(HomeToOverviewNudgeControllerTest, ShownLimitedNumberOfTimes) {
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(GetNudgeController());

  // Show the nudge kNotificationLimit amount of time.
  for (int i = 0; i < contextual_tooltip::kNotificationLimit; ++i) {
    SCOPED_TRACE(testing::Message() << "Attempt " << i);
    EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

    ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
    GetNudgeController()->FireShowTimerForTesting();
    ASSERT_TRUE(GetNudgeController()->nudge_for_testing());

    std::unique_ptr<aura::Window> window =
        CreateTestWindow(gfx::Rect(0, 0, 400, 400));
    wm::ActivateWindow(window.get());
    test_clock_.Advance(base::TimeDelta::FromHours(25));
    WindowState::Get(window.get())->Minimize();
  }

  // At this point, given the nudge was shown the intended number of times
  // already, the nudge should not show up, even though the device is on home
  // screen.
  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
}

// Tests that the nudge is hidden when tablet mode exits.
TEST_F(HomeToOverviewNudgeControllerTest, HiddenOnTabletModeExit) {
  TabletModeControllerTestApi().EnterTabletMode();

  ASSERT_TRUE(GetNudgeController());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(gfx::Transform(),
            GetHotseatWidget()->GetLayer()->GetTargetTransform());
}

// Tests that the nudge show is canceled when tablet mode exits.
TEST_F(HomeToOverviewNudgeControllerTest, ShowCanceledOnTabletModeExit) {
  TabletModeControllerTestApi().EnterTabletMode();

  ASSERT_TRUE(GetNudgeController());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(gfx::Transform(),
            GetHotseatWidget()->GetLayer()->GetTargetTransform());
}

// Tests that the nudge show animation is canceled when tablet mode exits.
TEST_F(HomeToOverviewNudgeControllerTest,
       ShowAnimationCanceledOnTabletModeExit) {
  TabletModeControllerTestApi().EnterTabletMode();

  ASSERT_TRUE(GetNudgeController());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  GetNudgeController()->FireShowTimerForTesting();
  ASSERT_TRUE(GetNudgeWidget()->GetLayer()->GetAnimator()->is_animating());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(gfx::Transform(),
            GetHotseatWidget()->GetLayer()->GetTargetTransform());
}

// Tests that the nudge is hidden when the screen is locked.
TEST_F(HomeToOverviewNudgeControllerTest, HiddenOnScreenLock) {
  TabletModeControllerTestApi().EnterTabletMode();

  ASSERT_TRUE(GetNudgeController());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(gfx::Transform(),
            GetHotseatWidget()->GetLayer()->GetTargetTransform());
}

// Tests that the nudge show is canceled if the in-app shelf is shown before the
// show timer runs.
TEST_F(HomeToOverviewNudgeControllerTest, InAppShelfShownBeforeShowTimer) {
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(GetNudgeController());

  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());

  // When the home screen is shown the next time, the nudge should be shown
  // again, without timeout to hide it.
  WindowState::Get(window.get())->Minimize();
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();
  EXPECT_TRUE(GetNudgeController()->nudge_for_testing());

  EXPECT_FALSE(GetNudgeController()->HasHideTimerForTesting());
}

// Tests that in-app shelf will hide the nudge if it happens during the
// animation to show the nudge.
TEST_F(HomeToOverviewNudgeControllerTest, NudgeHiddenDuringShowAnimation) {
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(GetNudgeController());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  GetNudgeController()->FireShowTimerForTesting();
  ASSERT_TRUE(GetNudgeWidget()->GetLayer()->GetAnimator()->is_animating());

  // Cache the widget, as GetNudgeWidget() will start returning nullptr when the
  // nudge starts hiding.
  ContextualNudge* nudge = GetNudgeController()->nudge_for_testing();
  views::Widget* nudge_widget = nudge->GetWidget();
  WidgetCloseObserver widget_close_observer(nudge_widget);

  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  EXPECT_FALSE(GetNudgeWidget());
  ASSERT_TRUE(nudge_widget->GetLayer()->GetAnimator()->is_animating());
  EXPECT_TRUE(nudge_widget->IsVisible());
  EXPECT_EQ(gfx::Transform(), nudge_widget->GetLayer()->GetTargetTransform());
  EXPECT_EQ(gfx::Transform(),
            GetHotseatWidget()->GetLayer()->GetTargetTransform());

  EXPECT_FALSE(widget_close_observer.WidgetClosed());

  ASSERT_TRUE(nudge->label()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(0.0f, nudge->label()->layer()->GetTargetOpacity());
  nudge->label()->layer()->GetAnimator()->StopAnimating();
  EXPECT_TRUE(widget_close_observer.WidgetClosed());

  EXPECT_TRUE(GetHotseatWidget()->GetLayer()->GetAnimator()->is_animating());
  EXPECT_EQ(gfx::Transform(),
            GetHotseatWidget()->GetLayer()->GetTargetTransform());

  // When the nudge is shown again, it should be hidden after a timeout.
  test_clock_.Advance(base::TimeDelta::FromHours(25));
  WindowState::Get(window.get())->Minimize();
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();
  EXPECT_TRUE(GetNudgeController()->nudge_for_testing());

  EXPECT_TRUE(GetNudgeController()->HasHideTimerForTesting());
}

// Tests that there is no crash if the nudge widget gets closed unexpectedly.
TEST_F(HomeToOverviewNudgeControllerTest, NoCrashIfNudgeWidgetGetsClosed) {
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(GetNudgeController());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  GetNudgeController()->FireShowTimerForTesting();
  GetNudgeWidget()->CloseNow();
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
}

// Tests that nudge and hotseat get repositioned appropriatelly if the display
// bounds change.
TEST_F(HomeToOverviewNudgeControllerTest,
       NudgeBoundsUpdatedOnDisplayBoundsChange) {
  UpdateDisplay("768x1200");
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(GetNudgeController());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();

  {
    SCOPED_TRACE("Initial bounds");
    SanityCheckNudgeBounds();
  }

  UpdateDisplay("1200x768");

  {
    SCOPED_TRACE("Updated bounds");
    SanityCheckNudgeBounds();
  }
}

}  // namespace ash
