// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_contextual_nudge_controller_impl.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/gestures/back_gesture/back_gesture_contextual_nudge.h"
#include "ash/wm/gestures/back_gesture/test_back_gesture_contextual_nudge_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@test.com";
constexpr char kUser2Email[] = "user2@test.com";

}  // namespace

class BackGestureContextualNudgeControllerTest : public NoSessionAshTestBase {
 public:
  BackGestureContextualNudgeControllerTest() = default;
  BackGestureContextualNudgeControllerTest(
      const BackGestureContextualNudgeControllerTest&) = delete;
  BackGestureContextualNudgeControllerTest& operator=(
      const BackGestureContextualNudgeControllerTest&) = delete;

  ~BackGestureContextualNudgeControllerTest() override = default;

  // NoSessionAshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    scoped_feature_list_.InitAndEnableFeature(features::kContextualNudges);
    nudge_controller_ =
        std::make_unique<BackGestureContextualNudgeControllerImpl>();

    GetSessionControllerClient()->AddUserSession(kUser1Email);
    GetSessionControllerClient()->AddUserSession(kUser2Email);

    // Simulate login of user 1.
    SwitchActiveUser(kUser1Email);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);

    // Enter tablet mode.
    TabletModeControllerTestApi().EnterTabletMode();
  }

  void TearDown() override {
    nudge_controller_.reset();
    NoSessionAshTestBase::TearDown();
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  void WaitNudgeAnimationDone() {
    while (nudge()) {
      base::RunLoop run_loop;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(),
          base::TimeDelta::FromMilliseconds(100));
      run_loop.Run();
    }
  }

  PrefService* user1_perf_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  BackGestureContextualNudgeControllerImpl* nudge_controller() {
    return nudge_controller_.get();
  }

  BackGestureContextualNudge* nudge() { return nudge_controller()->nudge(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<BackGestureContextualNudgeControllerImpl> nudge_controller_;
};

// Tests the timing when BackGestureContextualNudgeControllerImpl should monitor
// window activation changes.
TEST_F(BackGestureContextualNudgeControllerTest, MonitorWindowsTest) {
  // Only monitor windows in tablet mode.
  EXPECT_TRUE(nudge_controller()->is_monitoring_windows());
  TabletModeControllerTestApi tablet_mode_api;
  tablet_mode_api.LeaveTabletMode();
  EXPECT_FALSE(nudge_controller()->is_monitoring_windows());
  tablet_mode_api.EnterTabletMode();
  EXPECT_TRUE(nudge_controller()->is_monitoring_windows());

  // Only monitor windows in active user session.
  GetSessionControllerClient()->LockScreen();
  EXPECT_FALSE(nudge_controller()->is_monitoring_windows());
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(nudge_controller()->is_monitoring_windows());

  // Exit tablet mode for kUser1Email.
  tablet_mode_api.LeaveTabletMode();
  EXPECT_FALSE(nudge_controller()->is_monitoring_windows());
  // Then enter tablet mode for kUserEmail2.
  SwitchActiveUser(kUser2Email);
  tablet_mode_api.EnterTabletMode();
  EXPECT_TRUE(nudge_controller()->is_monitoring_windows());
}

// Tests the activation of another window will cancel the in-waiting or
// in-progress nudge animation.
TEST_F(BackGestureContextualNudgeControllerTest,
       ActivationCancelAnimationTest) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  EXPECT_FALSE(nudge());

  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_perf_service(), contextual_tooltip::TooltipType::kBackGesture));

  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  EXPECT_TRUE(nudge());
  EXPECT_TRUE(nudge()->widget()->GetLayer()->GetAnimator()->is_animating());

  // At this moment, change window activation should cancel the previous nudge
  // showup animation on |window1|, and start show nudge on |window2|.
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  EXPECT_FALSE(nudge()->ShouldNudgeCountAsShown());
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_perf_service(), contextual_tooltip::TooltipType::kBackGesture));
  EXPECT_TRUE(nudge()->widget()->GetLayer()->GetAnimator()->is_animating());

  // Wait until nudge animation is finished.
  WaitNudgeAnimationDone();
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      user1_perf_service(), contextual_tooltip::TooltipType::kBackGesture));
}

// Test that ending tablet mode will cancel in-waiting or in-progress nudge
// animation.
TEST_F(BackGestureContextualNudgeControllerTest,
       EndTabletModeCancelAnimationTest) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  EXPECT_FALSE(nudge());

  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_perf_service(), contextual_tooltip::TooltipType::kBackGesture));

  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_TRUE(nudge());
  EXPECT_TRUE(nudge()->widget()->GetLayer()->GetAnimator()->is_animating());

  TabletModeControllerTestApi().LeaveTabletMode();
  WaitNudgeAnimationDone();
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_perf_service(), contextual_tooltip::TooltipType::kBackGesture));
}

// Do not show nudge ui on window that can't perform "go back" operation.
TEST_F(BackGestureContextualNudgeControllerTest, CanNotGoBackWindowTest) {
  ash_test_helper()->test_shell_delegate()->SetCanGoBack(false);

  EXPECT_FALSE(nudge());
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_perf_service(), contextual_tooltip::TooltipType::kBackGesture));

  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_FALSE(nudge());
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_perf_service(), contextual_tooltip::TooltipType::kBackGesture));
}

}  // namespace ash
