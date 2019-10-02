// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_permissions_util.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/previews/core/previews_switches.h"

class OptimizationGuidePermissionsUtilTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    drp_test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .WithMockConfig()
            .Build();
    drp_test_context_->DisableWarmupURLFetch();
  }

  void TearDown() override {
    drp_test_context_->DestroySettings();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetDataSaverEnabled(bool enabled) {
    drp_test_context_->SetDataReductionProxyEnabled(enabled);
  }

  void SetInfobarSeen(bool has_seen_infobar) {
    // Make sure infobar not shown.
    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(profile());
    PreviewsLitePageRedirectDecider* decider =
        previews_service->previews_lite_page_redirect_decider();
    // Initialize settings here so Lite Pages Decider checks for the Data Saver
    // bit.
    decider->OnSettingsInitialized();
    if (has_seen_infobar) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          previews::switches::kDoNotRequireLitePageRedirectInfoBar);
    } else {
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          previews::switches::kDoNotRequireLitePageRedirectInfoBar);
    }
  }

 private:
  std::unique_ptr<data_reduction_proxy::DataReductionProxyTestContext>
      drp_test_context_;
};

TEST_F(OptimizationGuidePermissionsUtilTest, CreateIfAllowedNonDataSaverUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      {optimization_guide::features::kOptimizationHintsFetching});
  SetDataSaverEnabled(false);
  SetInfobarSeen(true);

  EXPECT_FALSE(IsUserPermittedToFetchHints(profile()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       CreateIfAllowedDataSaverUserInfobarNotSeen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      {optimization_guide::features::kOptimizationHintsFetching});
  SetDataSaverEnabled(true);
  SetInfobarSeen(false);

  EXPECT_FALSE(IsUserPermittedToFetchHints(profile()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsDataSaverUserInfobarSeen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      {optimization_guide::features::kOptimizationHintsFetching});
  SetDataSaverEnabled(true);
  SetInfobarSeen(true);

  EXPECT_TRUE(IsUserPermittedToFetchHints(profile()));
}

TEST_F(
    OptimizationGuidePermissionsUtilTest,
    IsUserPermittedToFetchHintsDataSaverUserInfobarSeenButHintsFetchingNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {optimization_guide::features::kOptimizationHintsFetching});
  SetDataSaverEnabled(true);
  SetInfobarSeen(true);

  EXPECT_FALSE(IsUserPermittedToFetchHints(profile()));
}
