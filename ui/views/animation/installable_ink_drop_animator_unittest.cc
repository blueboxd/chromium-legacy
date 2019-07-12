// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop_animator.h"

#include "base/bind_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/installable_ink_drop_painter.h"

namespace views {

class InstallableInkDropAnimatorTest : public ::testing::Test {
 public:
  InstallableInkDropAnimatorTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {}

  base::test::ScopedTaskEnvironment* scoped_task_environment() {
    return &scoped_task_environment_;
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(InstallableInkDropAnimatorTest, UpdatesTargetState) {
  InstallableInkDropPainter painter;
  InstallableInkDropAnimator animator(&painter, nullptr, base::DoNothing());
  EXPECT_EQ(InkDropState::HIDDEN, animator.target_state());

  animator.AnimateToState(InkDropState::ACTIVATED);
  EXPECT_EQ(InkDropState::ACTIVATED, animator.target_state());
}

TEST_F(InstallableInkDropAnimatorTest, InstantAnimationsFinishImmediately) {
  InstallableInkDropPainter painter;

  bool callback_called = false;
  base::RepeatingClosure callback = base::Bind(
      [](bool* callback_called) { *callback_called = true; }, &callback_called);
  InstallableInkDropAnimator animator(&painter, nullptr, callback);
  EXPECT_EQ(InkDropState::HIDDEN, animator.target_state());
  EXPECT_FALSE(painter.activated());
  EXPECT_FALSE(callback_called);

  animator.AnimateToState(InkDropState::ACTION_PENDING);
  EXPECT_EQ(InkDropState::ACTION_PENDING, animator.target_state());
  EXPECT_TRUE(painter.activated());
  EXPECT_TRUE(callback_called);

  callback_called = false;
  animator.AnimateToState(InkDropState::ACTION_TRIGGERED);
  EXPECT_EQ(InkDropState::HIDDEN, animator.target_state());
  EXPECT_FALSE(painter.activated());
  EXPECT_TRUE(callback_called);

  callback_called = false;
  animator.AnimateToState(InkDropState::ACTIVATED);
  EXPECT_EQ(InkDropState::ACTIVATED, animator.target_state());
  EXPECT_TRUE(painter.activated());
  EXPECT_TRUE(callback_called);

  callback_called = false;
  animator.AnimateToState(InkDropState::DEACTIVATED);
  EXPECT_EQ(InkDropState::HIDDEN, animator.target_state());
  EXPECT_FALSE(painter.activated());
  EXPECT_TRUE(callback_called);
}

TEST_F(InstallableInkDropAnimatorTest,
       TriggeredAnimationDelaysTransitionToHidden) {
  InstallableInkDropPainter painter;

  bool callback_called = false;
  base::RepeatingClosure callback = base::Bind(
      [](bool* callback_called) { *callback_called = true; }, &callback_called);
  InstallableInkDropAnimator animator(&painter, nullptr, callback);
  EXPECT_EQ(InkDropState::HIDDEN, animator.target_state());
  EXPECT_FALSE(painter.activated());
  EXPECT_FALSE(callback_called);

  animator.AnimateToState(InkDropState::ACTION_TRIGGERED);
  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, animator.target_state());
  EXPECT_TRUE(painter.activated());
  EXPECT_TRUE(callback_called);

  callback_called = false;
  scoped_task_environment()->FastForwardBy(
      InstallableInkDropAnimator::kAnimationDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(InkDropState::HIDDEN, animator.target_state());
  EXPECT_FALSE(painter.activated());
  EXPECT_TRUE(callback_called);
}

TEST_F(InstallableInkDropAnimatorTest, HighlightAnimationFadesInAndOut) {
  InstallableInkDropPainter painter;

  auto animation_container = base::MakeRefCounted<gfx::AnimationContainer>();
  gfx::AnimationContainerTestApi animation_test_api(animation_container.get());

  bool callback_called = false;
  base::RepeatingClosure callback = base::Bind(
      [](bool* callback_called) { *callback_called = true; }, &callback_called);
  InstallableInkDropAnimator animator(&painter, animation_container.get(),
                                      callback);
  EXPECT_EQ(0.0f, painter.highlighted_ratio());
  EXPECT_FALSE(callback_called);

  animator.AnimateHighlight(true);
  EXPECT_EQ(0.0f, painter.highlighted_ratio());

  callback_called = false;
  animation_test_api.IncrementTime(
      InstallableInkDropAnimator::kAnimationDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1.0f, painter.highlighted_ratio());
  EXPECT_TRUE(callback_called);

  animator.AnimateHighlight(false);
  EXPECT_EQ(1.0f, painter.highlighted_ratio());

  callback_called = false;
  animation_test_api.IncrementTime(
      InstallableInkDropAnimator::kAnimationDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0.0f, painter.highlighted_ratio());
  EXPECT_TRUE(callback_called);
}

}  // namespace views
