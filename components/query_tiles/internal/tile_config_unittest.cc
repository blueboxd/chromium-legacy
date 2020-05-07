// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_config.h"

#include "base/test/scoped_feature_list.h"
#include "components/query_tiles/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace upboarding {

// Test to verify Finch parameters for enabled experiment group is read
// correctly.
TEST(TileConfigTest, FinchConfigEnabled) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> params = {
      {kExperimentTagKey, "1234"},
      {kBaseURLKey, "https://test.com"},
      {kExpireDurationKey, "100"},
      {kIsUnmeteredNetworkRequiredKey, "true"}};
  feature_list.InitAndEnableFeatureWithParameters(features::kQueryTiles,
                                                  params);
  EXPECT_EQ(TileConfig::GetQueryTilesServerUrl(),
            "https://test.com/v1/querytiles");
  EXPECT_TRUE(TileConfig::GetIsUnMeteredNetworkRequired());
  EXPECT_EQ(TileConfig::GetExperimentTag(), "1234");
  EXPECT_EQ(TileConfig::GetExpireDuration(), base::TimeDelta::FromSeconds(100));
}

// Test to verify that if not configured, default parameter values are used.
TEST(TileConfigTest, FinchConfigDefaultParameter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kQueryTiles);
  EXPECT_EQ(
      TileConfig::GetQueryTilesServerUrl(),
      "https://autopush-gsaprototype-pa.sandbox.googleapis.com/v1/querytiles");
  EXPECT_FALSE(TileConfig::GetIsUnMeteredNetworkRequired());
  EXPECT_TRUE(TileConfig::GetExperimentTag().empty());
  EXPECT_EQ(TileConfig::GetExpireDuration(),
            base::TimeDelta::FromSeconds(48 * 60 * 60));
}

}  // namespace upboarding
