// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_suggestions_controller_impl.h"

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/features.h"

namespace ash {

namespace {

using chromeos::assistant::prefs::AssistantOnboardingMode;

// AssistantSuggestionsControllerImplTest --------------------------------------

class AssistantSuggestionsControllerImplTest : public AssistantAshTestBase {
 public:
  AssistantSuggestionsControllerImplTest() = default;
  ~AssistantSuggestionsControllerImplTest() override = default;

  AssistantSuggestionsControllerImpl* controller() {
    return static_cast<AssistantSuggestionsControllerImpl*>(
        AssistantSuggestionsController::Get());
  }

  const AssistantSuggestionsModel* model() { return controller()->GetModel(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_F(AssistantSuggestionsControllerImplTest,
       ShouldNotHaveOnboardingSuggestionsWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      chromeos::assistant::features::kAssistantBetterOnboarding);

  for (int i = 0; i < static_cast<int>(AssistantOnboardingMode::kMaxValue);
       ++i) {
    const auto onboarding_mode = static_cast<AssistantOnboardingMode>(i);
    SetOnboardingMode(onboarding_mode);
    EXPECT_TRUE(model()->GetOnboardingSuggestions().empty());
  }
}

TEST_F(AssistantSuggestionsControllerImplTest,
       ShouldMaybeHaveOnboardingSuggestionsWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantBetterOnboarding);

  for (int i = 0; i < static_cast<int>(AssistantOnboardingMode::kMaxValue);
       ++i) {
    const auto onboarding_mode = static_cast<AssistantOnboardingMode>(i);
    SetOnboardingMode(onboarding_mode);
    EXPECT_EQ(model()->GetOnboardingSuggestions().empty(),
              onboarding_mode != AssistantOnboardingMode::kEducation);
  }
}

}  // namespace ash
