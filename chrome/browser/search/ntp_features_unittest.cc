// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_features.h"

#include <map>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_features {

TEST(NTPFeaturesTest, IsRealboxEnabled) {
  {
    base::test::ScopedFeatureList feature_list;
    EXPECT_FALSE(IsRealboxEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kRealbox);
    EXPECT_TRUE(IsRealboxEnabled());

    // Realbox is disabled when new search features are disabled.
    feature_list.Reset();
    feature_list.InitWithFeatures({kRealbox}, {omnibox::kNewSearchFeatures});
    EXPECT_FALSE(IsRealboxEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(omnibox::kZeroSuggestionsOnNTPRealbox);
    EXPECT_TRUE(IsRealboxEnabled());

    // Realbox is disabled when new search features are disabled.
    feature_list.Reset();
    feature_list.InitWithFeatures({omnibox::kZeroSuggestionsOnNTPRealbox},
                                  {omnibox::kNewSearchFeatures});
    EXPECT_FALSE(IsRealboxEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        omnibox::kReactiveZeroSuggestionsOnNTPRealbox);
    EXPECT_TRUE(IsRealboxEnabled());

    // Realbox is disabled when new search features are disabled.
    feature_list.Reset();
    feature_list.InitWithFeatures(
        {omnibox::kReactiveZeroSuggestionsOnNTPRealbox},
        {omnibox::kNewSearchFeatures});
    EXPECT_FALSE(IsRealboxEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    // Reactive zero-prefix suggestions in the NTP Omnibox.
    feature_list.InitAndEnableFeature(
        omnibox::kReactiveZeroSuggestionsOnNTPOmnibox);
    EXPECT_FALSE(IsRealboxEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    // zero-prefix suggestions are configured for the NTP Omnibox.
    feature_list.InitWithFeaturesAndParameters(
        {{omnibox::kOnFocusSuggestions,
          {{"ZeroSuggestVariant:7:*", "Does not matter"}}}},
        {});
    EXPECT_FALSE(IsRealboxEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    // zero-prefix suggestions are configured for the NTP Realbox.
    feature_list.InitWithFeaturesAndParameters(
        {{omnibox::kOnFocusSuggestions,
          {{"ZeroSuggestVariant:15:*", "Does not matter"}}}},
        {});
    EXPECT_TRUE(IsRealboxEnabled());

    // Realbox is disabled when new search features are disabled.
    feature_list.Reset();
    feature_list.InitWithFeaturesAndParameters(
        {{omnibox::kOnFocusSuggestions,
          {{"ZeroSuggestVariant:15:*", "Does not matter"}}}},
        {omnibox::kNewSearchFeatures});
    EXPECT_FALSE(IsRealboxEnabled());
  }
}

}  // namespace ntp_features
