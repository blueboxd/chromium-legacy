// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_ranker/tab_score_predictor.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_ranker {
namespace {

// This tests that the TabRanker predictor returns a same score that is
// calcuated when the model is trained.
class TabScorePredictorTest : public testing::Test {
 public:
  TabScorePredictorTest() = default;
  ~TabScorePredictorTest() override = default;

 protected:
  // Returns a prediction for the tab example.
  float ScoreTab(const TabFeatures& tab) {
    float score = 0;
    EXPECT_EQ(TabRankerResult::kSuccess,
              TabScorePredictor().ScoreTab(tab, &score));
    return score;
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabScorePredictorTest);
};

}  // namespace

// Checks the score for an example that we have calculated a known score for
// outside of Chrome.
TEST_F(TabScorePredictorTest, KnownScore) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "1"}});
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(ScoreTab(GetFullTabFeaturesForTesting()), -10.076081);
}

// Checks the score for a different example that we have calculated a known
// score for outside of Chrome. This example omits the optional features.
TEST_F(TabScorePredictorTest, KnownScoreMissingOptionalFeatures) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "1"}});
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(ScoreTab(GetPartialTabFeaturesForTesting()), 5.1401806);
}

// Checks the score for an example that we have calculated a known score for
// outside of Chrome.
TEST_F(TabScorePredictorTest, KnownScorePairwise) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "2"}});
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(ScoreTab(GetFullTabFeaturesForTesting()), -3.8852997);
}

// Checks the score for a different example that we have calculated a known
// score for outside of Chrome. This example omits the optional features.
TEST_F(TabScorePredictorTest, KnownScoreMissingOptionalFeaturesPairwise) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "2"}});
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(ScoreTab(GetPartialTabFeaturesForTesting()), 1.9675488);
}

TEST_F(TabScorePredictorTest, ScoreWithMRUScorer) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker,
      {{"scorer_type", "0"}, {"mru_scorer_penalty", "0.2345"}});
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(ScoreTab(GetFullTabFeaturesForTesting()), 0.13639774);
}

TEST_F(TabScorePredictorTest, ScoreWithDiscardPenalty) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "0"},
                             {"mru_scorer_penalty", "0.2345"},
                             {"discard_count_penalty", "0.2468"}});
  // Pre-calculated score using the generated model outside of Chrome.
  auto tab = GetFullTabFeaturesForTesting();
  EXPECT_FLOAT_EQ(ScoreTab(tab), 0.13639774);

  tab.discard_count = 3;
  EXPECT_FLOAT_EQ(ScoreTab(tab), 0.28599524);
}

}  // namespace tab_ranker
