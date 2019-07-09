// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/optimization_guide/optimization_guide_constants.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

TEST(OptimizationGuideFeaturesTest,
     TestGetOptimizationGuideServiceURLHTTPSOnly) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationHintsFetching,
      {{"optimization_guide_service_url", "http://NotAnHTTPSServer.com"}});

  EXPECT_EQ(features::GetOptimizationGuideServiceURL().spec(),
            kOptimizationGuideServiceDefaultURL);
  EXPECT_TRUE(
      features::GetOptimizationGuideServiceURL().SchemeIs(url::kHttpsScheme));
}

TEST(OptimizationGuideFeaturesTest,
     TestGetOptimizationGuideServiceURLViaFinch) {
  base::test::ScopedFeatureList scoped_feature_list;

  std::string optimization_guide_service_url = "https://finchserver.com/";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationHintsFetching,
      {{"optimization_guide_service_url", optimization_guide_service_url}});

  EXPECT_EQ(features::GetOptimizationGuideServiceURL().spec(),
            optimization_guide_service_url);
}

}  // namespace

}  // namespace optimization_guide
