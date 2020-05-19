// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/assistive_suggester.h"
#include "chrome/browser/chromeos/input_method/personal_info_suggester.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class AssistiveSuggesterTest : public testing::Test {
 protected:
  AssistiveSuggesterTest() { profile_ = std::make_unique<TestingProfile>(); }

  void SetUp() override {
    engine_ = std::make_unique<InputMethodEngine>();
    assistive_suggester_ =
        std::make_unique<AssistiveSuggester>(engine_.get(), profile_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AssistiveSuggester> assistive_suggester_;
  std::unique_ptr<InputMethodEngine> engine_;
};

TEST_F(AssistiveSuggesterTest,
       EmojiSuggestionPrefFalseFeatureFlagTrue_AssitiveFeatureEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {chromeos::features::kEmojiSuggestAddition},
      // disabled_features
      {chromeos::features::kAssistPersonalInfo});

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->SetPath(std::string(kEmojiSuggestAdditionEnabledPrefName),
                  base::Value(false));

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       EmojiSuggestionPrefTrueFeatureFlagTrue_AssitiveFeatureEnabledTrue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {chromeos::features::kEmojiSuggestAddition},
      // disabled_features
      {chromeos::features::kAssistPersonalInfo});

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->SetPath(std::string(kEmojiSuggestAdditionEnabledPrefName),
                  base::Value(true));

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

}  // namespace chromeos
