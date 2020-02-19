// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/contextual_tooltip.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace contextual_tooltip {

class ContextualTooltipTest : public AshTestBase,
                              public testing::WithParamInterface<bool> {
 public:
  ContextualTooltipTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          ash::features::kContextualNudges);

    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ash::features::kContextualNudges);
    }
  }
  ~ContextualTooltipTest() override = default;

  base::SimpleTestClock* clock() { return &test_clock_; }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    contextual_tooltip::OverrideClockForTesting(&test_clock_);
  }
  void TearDown() override {
    contextual_tooltip::ClearClockOverrideForTesting();
    AshTestBase::TearDown();
  }

  PrefService* GetPrefService() {
    return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestClock test_clock_;
};

using ContextualTooltipDisabledTest = ContextualTooltipTest;

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTooltipDisabledTest,
                         testing::Values(false));
INSTANTIATE_TEST_SUITE_P(All, ContextualTooltipTest, testing::Values(true));

// Checks that nudges are not shown when the feature flag is disabled.
TEST_P(ContextualTooltipDisabledTest, FeatureFlagDisabled) {
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(GetPrefService(),
                                                   TooltipType::kDragHandle));
}

TEST_P(ContextualTooltipTest, ShouldShowPersistentDragHandleNudge) {
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(GetPrefService(),
                                                  TooltipType::kDragHandle));
  EXPECT_TRUE(contextual_tooltip::GetNudgeTimeout(GetPrefService(),
                                                  TooltipType::kDragHandle)
                  .is_zero());
}

// Checks that drag handle nudge has a timeout if it is not the first time it is
// being shown.
TEST_P(ContextualTooltipTest, NonPersistentDragHandleNudgeTimeout) {
  for (int shown_count = 1;
       shown_count < contextual_tooltip::kNotificationLimit; shown_count++) {
    contextual_tooltip::HandleNudgeShown(GetPrefService(),
                                         TooltipType::kDragHandle);
    clock()->Advance(contextual_tooltip::kMinInterval);
    EXPECT_EQ(contextual_tooltip::GetNudgeTimeout(GetPrefService(),
                                                  TooltipType::kDragHandle),
              contextual_tooltip::kNudgeShowDuration);
  }
}

// Checks that drag handle nudge should be shown after kMinInterval has passed
// since the last time it was shown but not before the time interval has passed.
TEST_P(ContextualTooltipTest, ShouldShowTimedDragHandleNudge) {
  contextual_tooltip::HandleNudgeShown(GetPrefService(),
                                       TooltipType::kDragHandle);
  for (int shown_count = 1;
       shown_count < contextual_tooltip::kNotificationLimit; shown_count++) {
    EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(GetPrefService(),
                                                     TooltipType::kDragHandle));
    clock()->Advance(contextual_tooltip::kMinInterval / 2);
    EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(GetPrefService(),
                                                     TooltipType::kDragHandle));
    clock()->Advance(contextual_tooltip::kMinInterval / 2);
    EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(GetPrefService(),
                                                    TooltipType::kDragHandle));
    contextual_tooltip::HandleNudgeShown(GetPrefService(),
                                         TooltipType::kDragHandle);
  }
  clock()->Advance(contextual_tooltip::kMinInterval);
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(GetPrefService(),
                                                   TooltipType::kDragHandle));
}

// Tests that if the user has successfully performed the gesture for at least
// |kSuccessLimit|, the corresponding nudge should not be shown.
TEST_P(ContextualTooltipTest, ShouldNotShowNudgeAfterSuccessLimit) {
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(GetPrefService(),
                                                  TooltipType::kDragHandle));
  for (int success_count = 0; success_count < contextual_tooltip::kSuccessLimit;
       success_count++) {
    contextual_tooltip::HandleGesturePerformed(GetPrefService(),
                                               TooltipType::kDragHandle);
  }

  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(GetPrefService(),
                                                   TooltipType::kDragHandle));
}

// Contextual nudge logic specific to back gesture.
TEST_P(ContextualTooltipTest, BackGestureContextualNudge) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  // Only show back gesture nudge in tablet mode.
  wm::ActivateWindow(window1.get());
  EXPECT_EQ(contextual_tooltip::GetShownCount(GetPrefService(),
                                              TooltipType::kBackGesture),
            0);
  TabletModeControllerTestApi().EnterTabletMode();
  wm::ActivateWindow(window2.get());
  EXPECT_EQ(contextual_tooltip::GetShownCount(GetPrefService(),
                                              TooltipType::kBackGesture),
            1);

  // We can only show back gesture nudge ui on windows that can perform "go
  // back" operation.
  ash_test_helper()->test_shell_delegate()->SetCanGoBack(false);
  clock()->Advance(contextual_tooltip::kMinInterval);
  EXPECT_EQ(contextual_tooltip::GetShownCount(GetPrefService(),
                                              TooltipType::kBackGesture),
            1);
}

}  // namespace contextual_tooltip

}  // namespace ash
