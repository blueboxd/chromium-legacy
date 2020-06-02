// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_prefs.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using omnibox::IsSuggestionGroupIdHidden;
using omnibox::kToggleSuggestionGroupIdOffHistogram;
using omnibox::kToggleSuggestionGroupIdOnHistogram;
using omnibox::ToggleSuggestionGroupIdVisibility;

class OmniboxPrefsTest : public ::testing::Test {
 public:
  OmniboxPrefsTest() = default;

  void SetUp() override {
    omnibox::RegisterProfilePrefs(GetPrefs()->registry());
  }

  TestingPrefServiceSimple* GetPrefs() { return &pref_service_; }

  base::HistogramTester* histogram() { return &histogram_; }

 private:
  TestingPrefServiceSimple pref_service_;
  base::HistogramTester histogram_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPrefsTest);
};

TEST_F(OmniboxPrefsTest, SuggestionGroupId) {
  const int kOnboardingGroupId = 40001;
  const int kRZPSGroupId = 40009;
  {
    // Expect |kOnboardingGroupId| to be visible.
    EXPECT_FALSE(IsSuggestionGroupIdHidden(GetPrefs(), kOnboardingGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOffHistogram, 0);

    // Expect |kRZPSGroupId| to be visible.
    EXPECT_FALSE(IsSuggestionGroupIdHidden(GetPrefs(), kRZPSGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOnHistogram, 0);
  }
  {
    ToggleSuggestionGroupIdVisibility(GetPrefs(), kOnboardingGroupId);

    // Expect |kOnboardingGroupId| to have been toggled hidden.
    EXPECT_TRUE(IsSuggestionGroupIdHidden(GetPrefs(), kOnboardingGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOffHistogram, 1);
    histogram()->ExpectBucketCount(kToggleSuggestionGroupIdOffHistogram,
                                   kOnboardingGroupId, 1);

    // Expect |kRZPSGroupId| to have remained visible.
    EXPECT_FALSE(IsSuggestionGroupIdHidden(GetPrefs(), kRZPSGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOnHistogram, 0);
  }
  {
    ToggleSuggestionGroupIdVisibility(GetPrefs(), kOnboardingGroupId);
    ToggleSuggestionGroupIdVisibility(GetPrefs(), kRZPSGroupId);

    // Expect |kRZPSGroupId| to have been toggled hidden.
    EXPECT_TRUE(IsSuggestionGroupIdHidden(GetPrefs(), kRZPSGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOffHistogram, 2);
    histogram()->ExpectBucketCount(kToggleSuggestionGroupIdOffHistogram,
                                   kRZPSGroupId, 1);

    // Expect |kOnboardingGroupId| to have been toggled visible again.
    EXPECT_FALSE(IsSuggestionGroupIdHidden(GetPrefs(), kOnboardingGroupId));
    histogram()->ExpectTotalCount(kToggleSuggestionGroupIdOnHistogram, 1);
    histogram()->ExpectBucketCount(kToggleSuggestionGroupIdOnHistogram,
                                   kOnboardingGroupId, 1);
  }
}
