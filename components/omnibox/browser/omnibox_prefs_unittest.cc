// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using omnibox::IsSuggestionGroupIdHidden;
using omnibox::ToggleSuggestionGroupIdVisibility;

class OmniboxPrefsTest : public ::testing::Test {
 public:
  OmniboxPrefsTest() = default;

  void SetUp() override {
    omnibox::RegisterProfilePrefs(GetPrefs()->registry());
  }

  TestingPrefServiceSimple* GetPrefs() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPrefsTest);
};

TEST_F(OmniboxPrefsTest, SuggestionGroupId) {
  const int kRecommendedForYouGroupId = 1;
  const int kRecentSearchesGroupId = 2;

  EXPECT_FALSE(
      IsSuggestionGroupIdHidden(GetPrefs(), kRecommendedForYouGroupId));
  EXPECT_FALSE(IsSuggestionGroupIdHidden(GetPrefs(), kRecentSearchesGroupId));

  ToggleSuggestionGroupIdVisibility(GetPrefs(), kRecommendedForYouGroupId);
  EXPECT_TRUE(IsSuggestionGroupIdHidden(GetPrefs(), kRecommendedForYouGroupId));
  EXPECT_FALSE(IsSuggestionGroupIdHidden(GetPrefs(), kRecentSearchesGroupId));

  ToggleSuggestionGroupIdVisibility(GetPrefs(), kRecommendedForYouGroupId);
  ToggleSuggestionGroupIdVisibility(GetPrefs(), kRecentSearchesGroupId);
  EXPECT_FALSE(
      IsSuggestionGroupIdHidden(GetPrefs(), kRecommendedForYouGroupId));
  EXPECT_TRUE(IsSuggestionGroupIdHidden(GetPrefs(), kRecentSearchesGroupId));
}
