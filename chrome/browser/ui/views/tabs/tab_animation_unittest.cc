// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_animation.h"

#include <utility>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr base::TimeDelta kZeroDuration = base::TimeDelta::FromMilliseconds(0);

}  // namespace

class TabAnimationTest : public testing::Test {
 public:
  static TabAnimation CreateAnimation(
      TabAnimationState initial_state,
      base::OnceClosure tab_removed_callback = base::BindOnce([]() {})) {
    return TabAnimation::ForStaticState(TabAnimation::ViewType::kTab,
                                        initial_state,
                                        std::move(tab_removed_callback));
  }

  TabAnimationTest()
      : env_(base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW) {
  }

  float PinnednessOf(TabAnimationState state) { return state.pinnedness_; }

  base::test::ScopedTaskEnvironment env_;
};

TEST_F(TabAnimationTest, StaticAnimationDoesNotChange) {
  TabAnimationState static_state = TabAnimationState::ForIdealTabState(
      TabAnimationState::TabOpenness::kOpen,
      TabAnimationState::TabPinnedness::kUnpinned,
      TabAnimationState::TabActiveness::kInactive, 0);
  TabAnimation static_animation = CreateAnimation(static_state);

  EXPECT_EQ(kZeroDuration, static_animation.GetTimeRemaining());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0),
            static_animation.GetTimeRemaining());
  EXPECT_EQ(PinnednessOf(static_state),
            PinnednessOf(static_animation.GetCurrentState()));

  env_.FastForwardBy(TabAnimation::kAnimationDuration);
  EXPECT_EQ(PinnednessOf(static_state),
            PinnednessOf(static_animation.GetCurrentState()));
}

TEST_F(TabAnimationTest, AnimationAnimates) {
  TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
      TabAnimationState::TabOpenness::kOpen,
      TabAnimationState::TabPinnedness::kUnpinned,
      TabAnimationState::TabActiveness::kInactive, 0);
  TabAnimationState target_state =
      initial_state.WithPinnedness(TabAnimationState::TabPinnedness::kPinned);
  TabAnimation animation = CreateAnimation(initial_state);
  animation.AnimateTo(target_state);

  EXPECT_LT(kZeroDuration, animation.GetTimeRemaining());
  EXPECT_EQ(PinnednessOf(initial_state),
            PinnednessOf(animation.GetCurrentState()));

  env_.FastForwardBy(TabAnimation::kAnimationDuration / 2.0);

  EXPECT_LT(kZeroDuration, animation.GetTimeRemaining());
  EXPECT_LT(PinnednessOf(initial_state),
            PinnednessOf(animation.GetCurrentState()));
  EXPECT_LT(PinnednessOf(animation.GetCurrentState()),
            PinnednessOf(target_state));

  env_.FastForwardBy(TabAnimation::kAnimationDuration / 2.0);

  EXPECT_EQ(PinnednessOf(target_state),
            PinnednessOf(animation.GetCurrentState()));
}

TEST_F(TabAnimationTest, CompletedAnimationSnapsToTarget) {
  TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
      TabAnimationState::TabOpenness::kOpen,
      TabAnimationState::TabPinnedness::kUnpinned,
      TabAnimationState::TabActiveness::kInactive, 0);
  TabAnimationState target_state =
      initial_state.WithPinnedness(TabAnimationState::TabPinnedness::kPinned);
  TabAnimation animation = CreateAnimation(initial_state);
  animation.AnimateTo(target_state);

  animation.CompleteAnimation();

  EXPECT_EQ(kZeroDuration, animation.GetTimeRemaining());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0), animation.GetTimeRemaining());
  EXPECT_EQ(PinnednessOf(target_state),
            PinnednessOf(animation.GetCurrentState()));
}

TEST_F(TabAnimationTest, ReplacedAnimationRestartsDuration) {
  TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
      TabAnimationState::TabOpenness::kOpen,
      TabAnimationState::TabPinnedness::kUnpinned,
      TabAnimationState::TabActiveness::kInactive, 0);
  TabAnimationState target_state =
      initial_state.WithPinnedness(TabAnimationState::TabPinnedness::kPinned);
  TabAnimation animation = CreateAnimation(initial_state);
  animation.AnimateTo(target_state);

  env_.FastForwardBy(TabAnimation::kAnimationDuration / 2.0);
  TabAnimationState reversal_state = animation.GetCurrentState();
  animation.AnimateTo(initial_state);

  EXPECT_EQ(PinnednessOf(reversal_state),
            PinnednessOf(animation.GetCurrentState()));

  EXPECT_EQ(TabAnimation::kAnimationDuration, animation.GetTimeRemaining());
}

TEST_F(TabAnimationTest, RetargetedAnimationKeepsDuration) {
  TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
      TabAnimationState::TabOpenness::kOpen,
      TabAnimationState::TabPinnedness::kUnpinned,
      TabAnimationState::TabActiveness::kInactive, 0);
  TabAnimationState target_state =
      initial_state.WithPinnedness(TabAnimationState::TabPinnedness::kPinned);
  TabAnimation animation = CreateAnimation(initial_state);
  animation.AnimateTo(target_state);

  env_.FastForwardBy(TabAnimation::kAnimationDuration / 2.0);
  EXPECT_EQ(TabAnimation::kAnimationDuration / 2.0,
            animation.GetTimeRemaining());
  animation.RetargetTo(initial_state);

  EXPECT_EQ(TabAnimation::kAnimationDuration / 2.0,
            animation.GetTimeRemaining());

  env_.FastForwardBy(TabAnimation::kAnimationDuration);
  EXPECT_EQ(PinnednessOf(initial_state),
            PinnednessOf(animation.GetCurrentState()));
}

TEST_F(TabAnimationTest, TestNotifyCloseCompleted) {
  class TabClosedDetector {
   public:
    void NotifyTabClosed() { was_closed_ = true; }

    bool was_closed_ = false;
  };
  TabAnimationState static_state = TabAnimationState::ForIdealTabState(
      TabAnimationState::TabOpenness::kOpen,
      TabAnimationState::TabPinnedness::kUnpinned,
      TabAnimationState::TabActiveness::kInactive, 0);
  TabClosedDetector tab_closed_detector;
  TabAnimation animation = CreateAnimation(
      static_state, base::BindOnce(&TabClosedDetector::NotifyTabClosed,
                                   base::Unretained(&tab_closed_detector)));
  EXPECT_FALSE(tab_closed_detector.was_closed_);

  animation.NotifyCloseCompleted();

  EXPECT_TRUE(tab_closed_detector.was_closed_);
}
