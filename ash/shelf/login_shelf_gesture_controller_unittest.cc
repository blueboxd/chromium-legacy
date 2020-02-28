// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_gesture_controller.h"

#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/shelf/contextual_nudge.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"

namespace ash {

class TestLoginShelfFlingHandler {
 public:
  TestLoginShelfFlingHandler() {
    gesture_detection_active_ =
        Shell::Get()->login_screen_controller()->SetLoginShelfGestureHandler(
            base::ASCIIToUTF16("Test swipe"),
            base::BindRepeating(&TestLoginShelfFlingHandler::OnFlingDetected,
                                base::Unretained(this)),
            base::BindOnce(
                &TestLoginShelfFlingHandler::OnGestureDetectionDisabled,
                base::Unretained(this)));
  }
  ~TestLoginShelfFlingHandler() {
    if (gesture_detection_active_)
      Shell::Get()->login_screen_controller()->ClearLoginShelfGestureHandler();
  }

  int GetAndResetDetectedFlingCount() {
    int result = detected_flings_;
    detected_flings_ = 0;
    return result;
  }

  bool gesture_detection_active() const { return gesture_detection_active_; }

  void OnFlingDetected() { ++detected_flings_; }

  void OnGestureDetectionDisabled() {
    EXPECT_TRUE(gesture_detection_active_);
    gesture_detection_active_ = false;
  }

 private:
  int detected_flings_ = 0;
  bool gesture_detection_active_ = false;
};

class LoginShelfGestureControllerTest : public LoginTestBase {
 public:
  LoginShelfGestureControllerTest() = default;
  LoginShelfGestureControllerTest(
      const LoginShelfGestureControllerTest& other) = delete;
  LoginShelfGestureControllerTest& operator=(
      const LoginShelfGestureControllerTest& other) = delete;
  ~LoginShelfGestureControllerTest() override = default;

  // LoginTestBase:
  void SetUp() override {
    set_start_session(false);
    LoginTestBase::SetUp();
  }

  LoginShelfGestureController* GetLoginScreenGestureController() {
    return GetPrimaryShelf()
        ->shelf_widget()
        ->login_shelf_gesture_controller_for_testing();
  }

  ContextualNudge* GetGestureContextualNudge() {
    return GetLoginScreenGestureController()->nudge_for_testing();
  }

  void NotifySessionStateChanged(session_manager::SessionState state) {
    GetSessionControllerClient()->SetSessionState(state);
    GetSessionControllerClient()->FlushForTest();
  }

  void SwipeOnShelf(const gfx::Point& start, const gfx::Vector2d& direction) {
    const gfx::Point end(start + direction);
    const base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(500);
    const int kNumScrollSteps = 4;
    GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                               kNumScrollSteps);
  }

  void FlingOnShelf(const gfx::Point& start, const gfx::Vector2d& direction) {
    const gfx::Point end(start + direction);
    const base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(10);
    const int kNumScrollSteps = 4;
    GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                               kNumScrollSteps);
  }
};

TEST_F(LoginShelfGestureControllerTest,
       SettingGestureHandlerShowsDragHandleInOobe) {
  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  EXPECT_FALSE(
      GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetLoginScreenGestureController());

  // Login shelf gesture detection should not start if not in tablet mode.
  auto fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_FALSE(fling_handler->gesture_detection_active());

  TabletModeControllerTestApi().EnterTabletMode();

  // Enter tablet mode and create another scoped login shelf gesture handler,
  // and verify that makes the drag handle visible.
  fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetLoginScreenGestureController());
  ASSERT_TRUE(GetPrimaryShelf()
                  ->shelf_widget()
                  ->login_shelf_gesture_controller_for_testing()
                  ->nudge_for_testing());
  EXPECT_TRUE(GetGestureContextualNudge()->GetWidget()->IsVisible());

  // The drag handle should be removed once the user logs in.
  CreateUserSessions(1);
  EXPECT_FALSE(fling_handler->gesture_detection_active());
  EXPECT_FALSE(
      GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetLoginScreenGestureController());
}

TEST_F(LoginShelfGestureControllerTest,
       SettingGestureHandlerShowsDragHandleOnLogin) {
  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_FALSE(
      GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetLoginScreenGestureController());

  // Login shelf gesture detection should not start if not in tablet mode.
  auto fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_FALSE(fling_handler->gesture_detection_active());

  TabletModeControllerTestApi().EnterTabletMode();

  // Enter tablet mode and create another scoped login shelf gesture handler,
  // and verify that makes the drag handle visible.
  fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetLoginScreenGestureController());
  ASSERT_TRUE(GetGestureContextualNudge());
  EXPECT_TRUE(GetGestureContextualNudge()->GetWidget()->IsVisible());

  // The drag handle should be removed once the user logs in.
  CreateUserSessions(1);
  EXPECT_FALSE(fling_handler->gesture_detection_active());
  EXPECT_FALSE(
      GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetLoginScreenGestureController());
}

TEST_F(LoginShelfGestureControllerTest, TabletModeExitResetsGestureDetection) {
  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  EXPECT_FALSE(
      GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetLoginScreenGestureController());

  // Login shelf gesture detection should not start if not in tablet mode.
  auto fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_FALSE(fling_handler->gesture_detection_active());

  TabletModeControllerTestApi().EnterTabletMode();

  // Enter tablet mode and create another scoped login shelf gesture handler,
  // and verify that makes the drag handle visible.
  fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetLoginScreenGestureController());
  ASSERT_TRUE(GetGestureContextualNudge());
  EXPECT_TRUE(GetGestureContextualNudge()->GetWidget()->IsVisible());

  // The drag handle should be removed in clamshell.
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(fling_handler->gesture_detection_active());
  EXPECT_FALSE(
      GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetLoginScreenGestureController());
}

TEST_F(LoginShelfGestureControllerTest,
       DragHandleHiddenIfGestureHandlerIsReset) {
  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  TabletModeControllerTestApi().EnterTabletMode();

  // Login shelf gesture detection should not start if not in tablet mode.
  auto fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetLoginScreenGestureController());
  ASSERT_TRUE(GetGestureContextualNudge());
  EXPECT_TRUE(GetGestureContextualNudge()->GetWidget()->IsVisible());

  fling_handler.reset();
  EXPECT_FALSE(
      GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetLoginScreenGestureController());
}

TEST_F(LoginShelfGestureControllerTest,
       HandlerDoesNotReceiveEventsAfterGettingNotifiedOfControllerExit) {
  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  TabletModeControllerTestApi().EnterTabletMode();

  // Login shelf gesture detection should not start if not in tablet mode.
  auto fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetLoginScreenGestureController());
  ASSERT_TRUE(GetGestureContextualNudge());
  EXPECT_TRUE(GetGestureContextualNudge()->GetWidget()->IsVisible());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(fling_handler->gesture_detection_active());
  EXPECT_FALSE(
      GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetLoginScreenGestureController());

  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(
      GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetLoginScreenGestureController());

  // Swipe on the shelf should not be reported given that the handler was
  // notified that the gesture controller was disabled (on tablet mode exit).
  const gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();
  FlingOnShelf(shelf_bounds.CenterPoint(), gfx::Vector2d(0, -100));
  EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());
}

TEST_F(LoginShelfGestureControllerTest,
       RegisteringHandlerClearsThePreviousOne) {
  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  TabletModeControllerTestApi().EnterTabletMode();

  // Login shelf gesture detection should not start if not in tablet mode.
  auto fling_handler_1 = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler_1->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetLoginScreenGestureController());
  ASSERT_TRUE(GetGestureContextualNudge());
  EXPECT_TRUE(GetGestureContextualNudge()->GetWidget()->IsVisible());

  auto fling_handler_2 = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler_2->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetLoginScreenGestureController());
  ASSERT_TRUE(GetGestureContextualNudge());
  EXPECT_TRUE(GetGestureContextualNudge()->GetWidget()->IsVisible());
  EXPECT_FALSE(fling_handler_1->gesture_detection_active());

  // Only the second handler should be notified of a gesture.
  const gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();
  // Fling up on shelf, and verify the gesture is detected.
  FlingOnShelf(shelf_bounds.CenterPoint(), gfx::Vector2d(0, -100));
  EXPECT_EQ(1, fling_handler_2->GetAndResetDetectedFlingCount());
  EXPECT_EQ(0, fling_handler_1->GetAndResetDetectedFlingCount());
}

TEST_F(LoginShelfGestureControllerTest,
       GracefullyHandleNudgeWidgetDestruction) {
  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  TabletModeControllerTestApi().EnterTabletMode();

  // Login shelf gesture detection should not start if not in tablet mode.
  auto fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetLoginScreenGestureController());
  ASSERT_TRUE(GetGestureContextualNudge());
  EXPECT_TRUE(GetGestureContextualNudge()->GetWidget()->IsVisible());

  GetGestureContextualNudge()->GetWidget()->CloseNow();

  // The gestures should still lbe recorded, even if the nudge widget went away.
  const gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();
  // Fling up on shelf, and verify the gesture is detected.
  FlingOnShelf(shelf_bounds.CenterPoint(), gfx::Vector2d(0, -100));
  EXPECT_EQ(1, fling_handler->GetAndResetDetectedFlingCount());
}

TEST_F(LoginShelfGestureControllerTest, FlingDetectionInOobeFromShelf) {
  const gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();
  const std::vector<gfx::Point> starting_points = {
      shelf_bounds.CenterPoint(),
      shelf_bounds.left_center(),
      shelf_bounds.left_center() + gfx::Vector2d(20, 0),
      shelf_bounds.right_center(),
      shelf_bounds.right_center() + gfx::Vector2d(-20, 0),
      shelf_bounds.bottom_center(),
      shelf_bounds.bottom_left() + gfx::Vector2d(20, 0),
      shelf_bounds.bottom_right() + gfx::Vector2d(-20, 0),
      shelf_bounds.top_center(),
      shelf_bounds.origin() + gfx::Vector2d(20, 0),
      shelf_bounds.top_right() + gfx::Vector2d(-20, 0),
  };

  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  TabletModeControllerTestApi().EnterTabletMode();

  // Enter tablet mode and create another scoped login shelf gesture handler,
  // and verify that makes the drag handle visible.
  auto fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());

  for (const auto& start : starting_points) {
    SCOPED_TRACE(testing::Message()
                 << "Starting point " << start.ToString()
                 << " with shelf bounds " << shelf_bounds.ToString());

    // Slow upward swipe should not trigger gesture detection.
    SwipeOnShelf(start, gfx::Vector2d(0, -100));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());

    // Fling up on shelf, and verify the gesture is detected.
    FlingOnShelf(start, gfx::Vector2d(0, -100));

    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(1, fling_handler->GetAndResetDetectedFlingCount());

    // Fling down, nor swipe down should not be detected.
    SwipeOnShelf(start, gfx::Vector2d(0, 20));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());

    FlingOnShelf(start, gfx::Vector2d(0, 20));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());
  }
}

TEST_F(LoginShelfGestureControllerTest, FlingDetectionOnLoginScreenFromShelf) {
  const gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();
  const std::vector<gfx::Point> starting_points = {
      shelf_bounds.CenterPoint(),
      shelf_bounds.left_center(),
      shelf_bounds.left_center() + gfx::Vector2d(20, 0),
      shelf_bounds.right_center(),
      shelf_bounds.right_center() + gfx::Vector2d(-20, 0),
      shelf_bounds.bottom_center(),
      shelf_bounds.bottom_left() + gfx::Vector2d(20, 0),
      shelf_bounds.bottom_right() + gfx::Vector2d(-20, 0),
      shelf_bounds.top_center(),
      shelf_bounds.origin() + gfx::Vector2d(20, 0),
      shelf_bounds.top_right() + gfx::Vector2d(-20, 0),
  };

  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  TabletModeControllerTestApi().EnterTabletMode();

  // Enter tablet mode and create another scoped login shelf gesture handler,
  // and verify that makes the drag handle visible.
  auto fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());

  for (const auto& start : starting_points) {
    SCOPED_TRACE(testing::Message()
                 << "Starting point " << start.ToString()
                 << " with shelf bounds " << shelf_bounds.ToString());

    // Slow upward swipe should not trigger gesture detection.
    SwipeOnShelf(start, gfx::Vector2d(0, -100));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());

    // Fling up on shelf, and verify the gesture is detected.
    FlingOnShelf(start, gfx::Vector2d(0, -100));

    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(1, fling_handler->GetAndResetDetectedFlingCount());

    // Fling down, nor swipe down should not be detected.
    SwipeOnShelf(start, gfx::Vector2d(0, 20));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());

    FlingOnShelf(start, gfx::Vector2d(0, 20));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());
  }
}

TEST_F(LoginShelfGestureControllerTest, FlingFromAboveTheShelf) {
  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  TabletModeControllerTestApi().EnterTabletMode();

  // Enter tablet mode and create another scoped login shelf gesture handler,
  // and verify that makes the drag handle visible.
  auto fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());

  const gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();
  const std::vector<gfx::Point> starting_points = {
      shelf_bounds.top_center() + gfx::Vector2d(0, -1),
      shelf_bounds.origin() + gfx::Vector2d(20, -1),
      shelf_bounds.top_right() + gfx::Vector2d(-20, -1),
  };

  for (const auto& start : starting_points) {
    SCOPED_TRACE(testing::Message()
                 << "Starting point " << start.ToString()
                 << " with shelf bounds " << shelf_bounds.ToString());

    SwipeOnShelf(start, gfx::Vector2d(0, -100));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());

    FlingOnShelf(start, gfx::Vector2d(0, -100));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());

    SwipeOnShelf(start, gfx::Vector2d(0, 20));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());

    FlingOnShelf(start, gfx::Vector2d(0, 20));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());
  }
}

TEST_F(LoginShelfGestureControllerTest, FlingDoesNotLeaveShelf) {
  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  TabletModeControllerTestApi().EnterTabletMode();

  // Enter tablet mode and create another scoped login shelf gesture handler,
  // and verify that makes the drag handle visible.
  auto fling_handler = std::make_unique<TestLoginShelfFlingHandler>();
  EXPECT_TRUE(fling_handler->gesture_detection_active());
  EXPECT_TRUE(GetPrimaryShelf()->shelf_widget()->GetDragHandle()->GetVisible());

  const gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();
  const std::vector<gfx::Point> starting_points = {
      shelf_bounds.bottom_center(),
      shelf_bounds.bottom_left(),
      shelf_bounds.bottom_right(),
  };

  for (const auto& start : starting_points) {
    SCOPED_TRACE(testing::Message()
                 << "Starting point " << start.ToString()
                 << " with shelf bounds " << shelf_bounds.ToString());

    SwipeOnShelf(start, gfx::Vector2d(0, -20));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());

    FlingOnShelf(start, gfx::Vector2d(0, -20));
    EXPECT_TRUE(fling_handler->gesture_detection_active());
    EXPECT_EQ(0, fling_handler->GetAndResetDetectedFlingCount());
  }
}

// Tests that shutdown is graceful if a login shelf gesture handler is still
// registered.
TEST_F(LoginShelfGestureControllerTest, HandlerExitsOnShutdown) {
  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  TabletModeControllerTestApi().EnterTabletMode();

  Shell::Get()->login_screen_controller()->SetLoginShelfGestureHandler(
      base::ASCIIToUTF16("Test swipe"), base::DoNothing(), base::DoNothing());
}

}  // namespace ash
