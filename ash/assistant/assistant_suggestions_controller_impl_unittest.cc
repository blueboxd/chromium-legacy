// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_suggestions_controller_impl.h"

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/assistant/public/cpp/features.h"

namespace ash {

namespace {

// AssistantSuggestionsControllerImplTest --------------------------------------

class AssistantSuggestionsControllerImplTest
    : public AssistantAshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AssistantSuggestionsControllerImplTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          chromeos::assistant::features::kAssistantBetterOnboarding);
    } else {
      feature_list_.InitAndDisableFeature(
          chromeos::assistant::features::kAssistantBetterOnboarding);
    }
  }

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

TEST_P(AssistantSuggestionsControllerImplTest,
       ShouldMaybeHaveOnboardingSuggestionsOnCreation) {
  // The model should only have onboarding suggestions when enabled.
  EXPECT_NE(GetParam(), model()->GetOnboardingSuggestions().empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         AssistantSuggestionsControllerImplTest,
                         testing::Bool());

}  // namespace ash
