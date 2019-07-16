// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_animator.h"

#include <utility>

#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/tabs/tab_animation.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TabClosedDetector {
 public:
  void NotifyTabClosed() { was_closed_ = true; }
  bool was_closed_ = false;
};

}  // namespace

class TabStripAnimatorTest : public testing::Test {
 public:
  TabStripAnimatorTest()
      : env_(base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW),
        animator_(
            base::BindRepeating(&TabStripAnimatorTest::OnAnimationProgressed,
                                base::Unretained(this))),
        has_animated_(false) {}

  void OnAnimationProgressed() { has_animated_ = true; }

  float OpennessOf(TabAnimationState state) { return state.openness_; }
  float PinnednessOf(TabAnimationState state) { return state.pinnedness_; }
  float ActivenessOf(TabAnimationState state) { return state.activeness_; }

  void AddTab(int index,
              TabAnimationState::TabActiveness activeness,
              TabAnimationState::TabPinnedness pinnedness =
                  TabAnimationState::TabPinnedness::kUnpinned,
              base::OnceClosure tab_removed_callback = base::BindOnce([]() {
              })) {
    animator_.InsertTabAtNoAnimation(TabAnimation::ViewType::kTab, index,
                                     std::move(tab_removed_callback),
                                     activeness, pinnedness);
  }

  base::test::ScopedTaskEnvironment env_;
  TabStripAnimator animator_;
  bool has_animated_;
};

TEST_F(TabStripAnimatorTest, StaticStripIsNotAnimating) {
  AddTab(0, TabAnimationState::TabActiveness::kInactive);
  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_FALSE(has_animated_);
}

TEST_F(TabStripAnimatorTest, InsertTabAnimation) {
  animator_.InsertTabAt(TabAnimation::ViewType::kTab, 0,
                        base::BindOnce([]() {}),
                        TabAnimationState::TabActiveness::kActive,
                        TabAnimationState::TabPinnedness::kUnpinned);
  EXPECT_TRUE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(0.0f, OpennessOf(animator_.GetCurrentTabStates()[0]));

  env_.FastForwardBy(2 * TabAnimation::kAnimationDuration);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, OpennessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_TRUE(has_animated_);
}

TEST_F(TabStripAnimatorTest, ChangeActiveTab) {
  AddTab(0, TabAnimationState::TabActiveness::kActive);
  AddTab(1, TabAnimationState::TabActiveness::kInactive);
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_EQ(0.0f, ActivenessOf(animator_.GetCurrentTabStates()[1]));

  animator_.SetActiveTab(0, 1);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(0.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[1]));
}

TEST_F(TabStripAnimatorTest, PinAndUnpinTab) {
  AddTab(0, TabAnimationState::TabActiveness::kActive);
  EXPECT_EQ(0.0f, PinnednessOf(animator_.GetCurrentTabStates()[0]));

  animator_.SetPinnednessNoAnimation(0,
                                     TabAnimationState::TabPinnedness::kPinned);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1.0f, PinnednessOf(animator_.GetCurrentTabStates()[0]));

  animator_.SetPinnednessNoAnimation(
      0, TabAnimationState::TabPinnedness::kUnpinned);

  EXPECT_EQ(0.0f, PinnednessOf(animator_.GetCurrentTabStates()[0]));
}

TEST_F(TabStripAnimatorTest, RemoveTabNoAnimation) {
  AddTab(0, TabAnimationState::TabActiveness::kActive);
  AddTab(1, TabAnimationState::TabActiveness::kInactive);

  animator_.RemoveTabNoAnimation(1);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
}

TEST_F(TabStripAnimatorTest, RemoveTabAnimation) {
  AddTab(0, TabAnimationState::TabActiveness::kActive);
  TabClosedDetector second_tab;
  AddTab(1, TabAnimationState::TabActiveness::kInactive,
         TabAnimationState::TabPinnedness::kUnpinned,
         base::BindOnce(&TabClosedDetector::NotifyTabClosed,
                        base::Unretained(&second_tab)));

  animator_.RemoveTab(1);

  EXPECT_TRUE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, OpennessOf(animator_.GetCurrentTabStates()[1]));
  EXPECT_FALSE(second_tab.was_closed_);

  env_.FastForwardBy(2 * TabAnimation::kAnimationDuration);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_TRUE(second_tab.was_closed_);
}

TEST_F(TabStripAnimatorTest, CompleteAnimations) {
  animator_.InsertTabAt(TabAnimation::ViewType::kTab, 0,
                        base::BindOnce([]() {}),
                        TabAnimationState::TabActiveness::kActive,
                        TabAnimationState::TabPinnedness::kUnpinned);
  EXPECT_TRUE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(0.0f, OpennessOf(animator_.GetCurrentTabStates()[0]));

  animator_.CompleteAnimations();

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1.0f, OpennessOf(animator_.GetCurrentTabStates()[0]));
}

TEST_F(TabStripAnimatorTest, CompleteAnimationsRemovesClosedTabs) {
  AddTab(0, TabAnimationState::TabActiveness::kActive);
  TabClosedDetector second_tab;
  AddTab(1, TabAnimationState::TabActiveness::kInactive,
         TabAnimationState::TabPinnedness::kUnpinned,
         base::BindOnce(&TabClosedDetector::NotifyTabClosed,
                        base::Unretained(&second_tab)));

  animator_.RemoveTab(1);

  EXPECT_TRUE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, OpennessOf(animator_.GetCurrentTabStates()[1]));
  EXPECT_FALSE(second_tab.was_closed_);

  animator_.CompleteAnimations();

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_TRUE(second_tab.was_closed_);
}

TEST_F(TabStripAnimatorTest,
       CompleteAnimationsWithoutDestroyingTabsDoesNotRemoveClosedTabs) {
  AddTab(0, TabAnimationState::TabActiveness::kActive);
  TabClosedDetector second_tab;
  AddTab(1, TabAnimationState::TabActiveness::kInactive,
         TabAnimationState::TabPinnedness::kUnpinned,
         base::BindOnce(&TabClosedDetector::NotifyTabClosed,
                        base::Unretained(&second_tab)));

  animator_.RemoveTab(1);

  EXPECT_TRUE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, OpennessOf(animator_.GetCurrentTabStates()[1]));
  EXPECT_FALSE(second_tab.was_closed_);

  animator_.CompleteAnimationsWithoutDestroyingTabs();

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_FALSE(second_tab.was_closed_);
}

TEST_F(TabStripAnimatorTest, MoveTabsRight) {
  std::vector<int> tabs_closed;
  for (int i = 0; i < 5; i++) {
    AddTab(i, TabAnimationState::TabActiveness::kInactive,
           TabAnimationState::TabPinnedness::kPinned,
           base::BindLambdaForTesting(
               [&tabs_closed, i]() { tabs_closed.push_back(i); }));
  }

  animator_.MoveTabsNoAnimation({0, 1}, 2);

  // Close tabs to run their callbacks.
  while (animator_.animation_count() > 0) {
    animator_.RemoveTab(0);
    animator_.CompleteAnimations();
  }

  std::vector<int> expected_tabs_closed = {2, 3, 0, 1, 4};
  EXPECT_EQ(expected_tabs_closed.size(), tabs_closed.size());
  for (size_t i = 0; i < tabs_closed.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(expected_tabs_closed[i], tabs_closed[i]);
  }
}

TEST_F(TabStripAnimatorTest, MoveTabsLeft) {
  std::vector<int> tabs_closed;
  for (int i = 0; i < 5; i++) {
    AddTab(i, TabAnimationState::TabActiveness::kInactive,
           TabAnimationState::TabPinnedness::kPinned,
           base::BindLambdaForTesting(
               [&tabs_closed, i]() { tabs_closed.push_back(i); }));
  }

  animator_.MoveTabsNoAnimation({3, 4}, 1);

  // Close tabs to run their callbacks.
  while (animator_.animation_count() > 0) {
    animator_.RemoveTab(0);
    animator_.CompleteAnimations();
  }

  std::vector<int> expected_tabs_closed = {0, 3, 4, 1, 2};
  EXPECT_EQ(expected_tabs_closed.size(), tabs_closed.size());
  for (size_t i = 0; i < tabs_closed.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(expected_tabs_closed[i], tabs_closed[i]);
  }
}

TEST_F(TabStripAnimatorTest, MoveTabsSamePosition) {
  AddTab(0, TabAnimationState::TabActiveness::kActive);
  AddTab(1, TabAnimationState::TabActiveness::kInactive);

  animator_.MoveTabsNoAnimation({0}, 0);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_EQ(0.0f, ActivenessOf(animator_.GetCurrentTabStates()[1]));
}
