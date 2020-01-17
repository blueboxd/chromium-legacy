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
  // Enables the tab freeze feature, and sets up the associated variations
  // parameter values.
  void EnableTabFreeze() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kTabFreeze, params_);
  }

  void SetParam(base::StringPiece key, base::StringPiece value) {
    params_[key.as_string()] = value.as_string();
  }

  void ExpectTabFreezeParams(bool should_periodically_unfreeze,
                             base::TimeDelta freeze_timeout,
                             base::TimeDelta unfreeze_timeout,
                             base::TimeDelta refreeze_timeout,
                             bool freezing_protect_media_only) {
    TabFreezeParams params = GetTabFreezeParams();

    EXPECT_EQ(should_periodically_unfreeze,
              params.should_periodically_unfreeze);

    EXPECT_EQ(freeze_timeout, params.freeze_timeout);
    EXPECT_EQ(unfreeze_timeout, params.unfreeze_timeout);
    EXPECT_EQ(refreeze_timeout, params.refreeze_timeout);

    EXPECT_EQ(freezing_protect_media_only, params.freezing_protect_media_only);
  }

  void ExpectDefaultTabFreezeParams() {
    ExpectTabFreezeParams(
        TabFreezeParams::kShouldPeriodicallyUnfreeze.default_value,
        base::TimeDelta::FromSeconds(
            TabFreezeParams::kFreezeTimeout.default_value),
        base::TimeDelta::FromSeconds(
            TabFreezeParams::kUnfreezeTimeout.default_value),
        base::TimeDelta::FromSeconds(
            TabFreezeParams::kRefreezeTimeout.default_value),
        TabFreezeParams::kFreezingProtectMediaOnly.default_value);
  }

 private:
  base::FieldTrialParams params_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST_F(TabManagerFeaturesTest, GetTabFreezeParamsDisabledFeatureGoesToDefault) {
  // Do not enable the tab freeze feature.
  ExpectDefaultTabFreezeParams();
}

TEST_F(TabManagerFeaturesTest, GetTabFreezeParamsNoneGoesToDefault) {
  EnableTabFreeze();
  ExpectDefaultTabFreezeParams();
}

TEST_F(TabManagerFeaturesTest, GetTabFreezeParamsInvalidGoesToDefault) {
  SetParam(TabFreezeParams::kShouldPeriodicallyUnfreeze.name, "blah");
  SetParam(TabFreezeParams::kFreezeTimeout.name, "b");
  SetParam(TabFreezeParams::kUnfreezeTimeout.name, "i");
  SetParam(TabFreezeParams::kRefreezeTimeout.name, "m");
  SetParam(TabFreezeParams::kFreezingProtectMediaOnly.name, "bleh");
  EnableTabFreeze();
  ExpectDefaultTabFreezeParams();
}

TEST_F(TabManagerFeaturesTest, GetTabFreezeParams) {
  SetParam(TabFreezeParams::kShouldPeriodicallyUnfreeze.name, "true");
  SetParam(TabFreezeParams::kFreezeTimeout.name, "10");
  SetParam(TabFreezeParams::kUnfreezeTimeout.name, "20");
  SetParam(TabFreezeParams::kRefreezeTimeout.name, "30");
  SetParam(TabFreezeParams::kFreezingProtectMediaOnly.name, "true");
  EnableTabFreeze();

  ExpectTabFreezeParams(true, base::TimeDelta::FromSeconds(10),
                        base::TimeDelta::FromSeconds(20),
                        base::TimeDelta::FromSeconds(30), true);
}

}  // namespace resource_coordinator
