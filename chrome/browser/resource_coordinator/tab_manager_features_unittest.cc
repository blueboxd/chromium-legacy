// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_features.h"

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class TabManagerFeaturesTest : public testing::Test {
 public:
  // Enables the proactive tab discarding feature, and sets up the associated
  // variations parameter values.
  void EnableProactiveTabFreezeAndDiscard() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kProactiveTabFreezeAndDiscard, params_);
  }

  void SetParam(base::StringPiece key, base::StringPiece value) {
    params_[key.as_string()] = value.as_string();
  }

  void ExpectProactiveTabFreezeAndDiscardParams(
      bool should_proactively_discard,
      bool should_periodically_unfreeze,
      int low_loaded_tab_count,
      int moderate_loaded_tab_count,
      int high_loaded_tab_count,
      int memory_in_gb,
      base::TimeDelta low_occluded_timeout,
      base::TimeDelta moderate_occluded_timeout,
      base::TimeDelta high_occluded_timeout,
      base::TimeDelta freeze_timeout,
      base::TimeDelta unfreeze_timeout,
      base::TimeDelta refreeze_timeout,
      bool freezing_protect_media_only) {
    ProactiveTabFreezeAndDiscardParams params =
        GetProactiveTabFreezeAndDiscardParams(memory_in_gb);

    EXPECT_EQ(should_proactively_discard, params.should_proactively_discard);
    EXPECT_EQ(should_periodically_unfreeze,
              params.should_periodically_unfreeze);
    EXPECT_EQ(low_loaded_tab_count, params.low_loaded_tab_count);
    EXPECT_EQ(moderate_loaded_tab_count, params.moderate_loaded_tab_count);

    // Enforce that |moderate_loaded_tab_count| is within [low_loaded_tab_count,
    // high_loaded_tab_count].
    EXPECT_GE(params.moderate_loaded_tab_count, params.low_loaded_tab_count);
    EXPECT_LE(params.moderate_loaded_tab_count, params.high_loaded_tab_count);

    EXPECT_EQ(high_loaded_tab_count, params.high_loaded_tab_count);
    EXPECT_EQ(low_occluded_timeout, params.low_occluded_timeout);
    EXPECT_EQ(moderate_occluded_timeout, params.moderate_occluded_timeout);
    EXPECT_EQ(high_occluded_timeout, params.high_occluded_timeout);

    EXPECT_EQ(freeze_timeout, params.freeze_timeout);
    EXPECT_EQ(unfreeze_timeout, params.unfreeze_timeout);
    EXPECT_EQ(refreeze_timeout, params.refreeze_timeout);

    EXPECT_EQ(freezing_protect_media_only, params.freezing_protect_media_only);
  }

  void ExpectDefaultProactiveTabFreezeAndDiscardParams() {
    int memory_in_gb = 4;
    ExpectProactiveTabFreezeAndDiscardParams(
        ProactiveTabFreezeAndDiscardParams::kShouldProactivelyDiscard
            .default_value,
        ProactiveTabFreezeAndDiscardParams::kShouldPeriodicallyUnfreeze
            .default_value,
        ProactiveTabFreezeAndDiscardParams::kLowLoadedTabCount.default_value,
        ProactiveTabFreezeAndDiscardParams::kModerateLoadedTabsPerGbRam
                .default_value *
            memory_in_gb,
        ProactiveTabFreezeAndDiscardParams::kHighLoadedTabCount.default_value,
        memory_in_gb,
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kLowOccludedTimeout
                .default_value),
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kModerateOccludedTimeout
                .default_value),
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kHighOccludedTimeout
                .default_value),
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kFreezeTimeout.default_value),
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kUnfreezeTimeout.default_value),
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kRefreezeTimeout.default_value),
        ProactiveTabFreezeAndDiscardParams::kFreezingProtectMediaOnly
            .default_value);
  }

 private:
  base::FieldTrialParams params_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST_F(TabManagerFeaturesTest,
       GetProactiveTabFreezeAndDiscardParamsDisabledFeatureGoesToDefault) {
  // Do not enable the proactive tab discarding feature.
  ExpectDefaultProactiveTabFreezeAndDiscardParams();
}

TEST_F(TabManagerFeaturesTest,
       GetProactiveTabFreezeAndDiscardParamsNoneGoesToDefault) {
  EnableProactiveTabFreezeAndDiscard();
  ExpectDefaultProactiveTabFreezeAndDiscardParams();
}

TEST_F(TabManagerFeaturesTest,
       GetProactiveTabFreezeAndDiscardParamsInvalidGoesToDefault) {
  SetParam(ProactiveTabFreezeAndDiscardParams::kShouldProactivelyDiscard.name,
           "blah");
  SetParam(ProactiveTabFreezeAndDiscardParams::kShouldPeriodicallyUnfreeze.name,
           "blah");
  SetParam(ProactiveTabFreezeAndDiscardParams::kLowLoadedTabCount.name, "ab");
  SetParam(ProactiveTabFreezeAndDiscardParams::kModerateLoadedTabsPerGbRam.name,
           "27.8");
  SetParam(ProactiveTabFreezeAndDiscardParams::kHighLoadedTabCount.name, "4e8");
  SetParam(ProactiveTabFreezeAndDiscardParams::kLowOccludedTimeout.name, "---");
  SetParam(ProactiveTabFreezeAndDiscardParams::kModerateOccludedTimeout.name,
           " ");
  SetParam(ProactiveTabFreezeAndDiscardParams::kHighOccludedTimeout.name, "");
  SetParam(ProactiveTabFreezeAndDiscardParams::kFreezeTimeout.name, "b");
  SetParam(ProactiveTabFreezeAndDiscardParams::kUnfreezeTimeout.name, "i");
  SetParam(ProactiveTabFreezeAndDiscardParams::kRefreezeTimeout.name, "m");
  SetParam(ProactiveTabFreezeAndDiscardParams::kFreezingProtectMediaOnly.name,
           "bleh");
  EnableProactiveTabFreezeAndDiscard();
  ExpectDefaultProactiveTabFreezeAndDiscardParams();
}

TEST_F(TabManagerFeaturesTest, GetProactiveTabFreezeAndDiscardParams) {
  SetParam(ProactiveTabFreezeAndDiscardParams::kShouldProactivelyDiscard.name,
           "true");
  SetParam(ProactiveTabFreezeAndDiscardParams::kShouldPeriodicallyUnfreeze.name,
           "true");
  SetParam(ProactiveTabFreezeAndDiscardParams::kLowLoadedTabCount.name, "7");
  SetParam(ProactiveTabFreezeAndDiscardParams::kModerateLoadedTabsPerGbRam.name,
           "4");
  SetParam(ProactiveTabFreezeAndDiscardParams::kHighLoadedTabCount.name, "42");
  // These are expressed in seconds.
  SetParam(ProactiveTabFreezeAndDiscardParams::kLowOccludedTimeout.name, "60");
  SetParam(ProactiveTabFreezeAndDiscardParams::kModerateOccludedTimeout.name,
           "120");
  SetParam(ProactiveTabFreezeAndDiscardParams::kHighOccludedTimeout.name,
           "247");
  SetParam(ProactiveTabFreezeAndDiscardParams::kFreezeTimeout.name, "10");
  SetParam(ProactiveTabFreezeAndDiscardParams::kUnfreezeTimeout.name, "20");
  SetParam(ProactiveTabFreezeAndDiscardParams::kRefreezeTimeout.name, "30");
  SetParam(ProactiveTabFreezeAndDiscardParams::kFreezingProtectMediaOnly.name,
           "true");
  EnableProactiveTabFreezeAndDiscard();

  // Should snap |moderate_loaded_tab_count| to |low_loaded_tab_count|, when the
  // amount of physical memory is so low that (|memory_in_gb| *
  // |moderate_tab_count_per_gb_ram|) < |low_loaded_tab_count|).
  int memory_in_gb_low = 1;
  ExpectProactiveTabFreezeAndDiscardParams(
      true, true, 7, 7, 42, memory_in_gb_low, base::TimeDelta::FromSeconds(60),
      base::TimeDelta::FromSeconds(120), base::TimeDelta::FromSeconds(247),
      base::TimeDelta::FromSeconds(10), base::TimeDelta::FromSeconds(20),
      base::TimeDelta::FromSeconds(30), true);

  // Should snap |moderate_loaded_tab_count| to |high_loaded_tab_count|, when
  // the amount of physical memory is so high that (|memory_in_gb| *
  // |moderate_tab_count_per_gb_ram|) > |high_loaded_tab_count|).
  int memory_in_gb_high = 100;
  ExpectProactiveTabFreezeAndDiscardParams(
      true, true, 7, 42, 42, memory_in_gb_high,
      base::TimeDelta::FromSeconds(60), base::TimeDelta::FromSeconds(120),
      base::TimeDelta::FromSeconds(247), base::TimeDelta::FromSeconds(10),
      base::TimeDelta::FromSeconds(20), base::TimeDelta::FromSeconds(30), true);

  // Tests normal case where |memory_in gb| * |moderate_tab_count_per_gb_ram| is
  // in the interval [low_loaded_tab_count, high_loaded_tab_count].
  int memory_in_gb_normal = 4;
  ExpectProactiveTabFreezeAndDiscardParams(
      true, true, 7, 16, 42, memory_in_gb_normal,
      base::TimeDelta::FromSeconds(60), base::TimeDelta::FromSeconds(120),
      base::TimeDelta::FromSeconds(247), base::TimeDelta::FromSeconds(10),
      base::TimeDelta::FromSeconds(20), base::TimeDelta::FromSeconds(30), true);
}

}  // namespace resource_coordinator
