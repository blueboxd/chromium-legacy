// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_web_contents_observer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/proto/delay_async_script_execution_metadata.pb.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace optimization_guide {

// The base class of various browser tests for the Blink optimization guide.
// This provides the common test utilities.
class BlinkOptimizationGuideBrowserTestBase : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  BlinkOptimizationGuideWebContentsObserver* GetObserverForActiveWebContents() {
    return content::WebContentsUserData<
        BlinkOptimizationGuideWebContentsObserver>::
        FromWebContents(browser()->tab_strip_model()->GetActiveWebContents());
  }

  GURL GetURLWithMockHost(const std::string& relative_url) const {
    // The optimization guide service doesn't work with the localhost. Instead,
    // resolve the relative url with the mock host.
    return https_server_->GetURL("mock.host", relative_url);
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// The BlinkOptimizationGuideBrowserTest tests common behavior of optimization
// types for Blink (e.g., DELAY_ASYNC_SCRIPT_EXECUTION).
//
// This is designed to be optimization type independent. Add optimization type
// specific things to helper functions like ConstructionMetadata() instead of
// in the test body.
class BlinkOptimizationGuideBrowserTest
    : public BlinkOptimizationGuideBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<proto::OptimizationType, bool>> {
 public:
  BlinkOptimizationGuideBrowserTest() {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features{{features::kOptimizationHints, {{}}}};
    std::vector<base::Feature> disabled_features;

    // Initialize feature flags based on the optimization type.
    switch (GetOptimizationType()) {
      case proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION:
        if (IsFeatureFlagEnabled()) {
          std::map<std::string, std::string> parameters;
          parameters["delay_type"] = "use_optimization_guide";
          enabled_features.emplace_back(
              blink::features::kDelayAsyncScriptExecution, parameters);
        } else {
          disabled_features.push_back(
              blink::features::kDelayAsyncScriptExecution);
        }
        break;
      default:
        break;
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  // Constructs a fake optimization metadata based on the optimization type.
  OptimizationMetadata ConstructMetadata() {
    OptimizationMetadata optimization_guide_metadata;
    switch (GetOptimizationType()) {
      case proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION: {
        proto::DelayAsyncScriptExecutionMetadata metadata;
        metadata.set_delay_type(proto::DelayType::DELAY_TYPE_FINISHED_PARSING);
        optimization_guide_metadata.SetAnyMetadataForTesting(metadata);
        break;
      }
      default:
        break;
    }
    return optimization_guide_metadata;
  }

  // Returns the optimization type provided as the gtest parameter.
  proto::OptimizationType GetOptimizationType() {
    return std::get<0>(GetParam());
  }

  // Returns true if the feature flag for the optimization type is enabled. If
  // the optimization type doesn't have the feature flag, returns true. See
  // comments on instantiation for details.
  bool IsFeatureFlagEnabled() { return std::get<1>(GetParam()); }

  // Returns true if the hints for the optimization type is available.
  bool CheckIfHintsAvailable(blink::mojom::BlinkOptimizationGuideHints& hints) {
    switch (GetOptimizationType()) {
      case proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION:
        if (hints.delay_async_script_execution_hints) {
          EXPECT_EQ(blink::mojom::DelayAsyncScriptExecutionDelayType::
                        kFinishedParsing,
                    hints.delay_async_script_execution_hints->delay_type);
        }
        return !!hints.delay_async_script_execution_hints;
      default:
        return false;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Instantiates test cases for each optimization type.
INSTANTIATE_TEST_SUITE_P(
    All,
    BlinkOptimizationGuideBrowserTest,
    testing::Combine(
        // The optimization type.
        testing::Values(proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION),
        // Whether the feature flag for the optimization type is enabled.
        testing::Bool()));

IN_PROC_BROWSER_TEST_P(BlinkOptimizationGuideBrowserTest, Basic) {
  // Set up a fake optimization hints for simple.html.
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(GetURLWithMockHost("/simple.html"),
                          GetOptimizationType(), ConstructMetadata());

  // Navigation to the URL should see the hints as long as the optimization type
  // is enabled.
  ui_test_utils::NavigateToURL(browser(), GetURLWithMockHost("/simple.html"));
  {
    auto& hints = GetObserverForActiveWebContents()->sent_hints_for_testing();
    if (IsFeatureFlagEnabled()) {
      EXPECT_TRUE(CheckIfHintsAvailable(hints));
    } else {
      EXPECT_FALSE(CheckIfHintsAvailable(hints));
    }
  }

  // Navigation to the different URL shouldn't see the hints.
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost("/simple.html?different"));
  {
    auto& hints = GetObserverForActiveWebContents()->sent_hints_for_testing();
    EXPECT_FALSE(CheckIfHintsAvailable(hints));
  }

  // Navigation to the URL again should see the same hints as long as the
  // optimization guide is enabled.
  ui_test_utils::NavigateToURL(browser(), GetURLWithMockHost("/simple.html"));
  {
    auto& hints = GetObserverForActiveWebContents()->sent_hints_for_testing();
    if (IsFeatureFlagEnabled()) {
      EXPECT_TRUE(CheckIfHintsAvailable(hints));
    } else {
      EXPECT_FALSE(CheckIfHintsAvailable(hints));
    }
  }
}

IN_PROC_BROWSER_TEST_P(BlinkOptimizationGuideBrowserTest, NoMetadata) {
  // Set up a fake optimization hints without metadata for simple.html.
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(GetURLWithMockHost("/simple.html"),
                          GetOptimizationType(), base::nullopt);

  // Navigation to the URL shouldn't see the hints.
  ui_test_utils::NavigateToURL(browser(), GetURLWithMockHost("/simple.html"));
  blink::mojom::BlinkOptimizationGuideHints& hints =
      GetObserverForActiveWebContents()->sent_hints_for_testing();
  EXPECT_FALSE(CheckIfHintsAvailable(hints));
}

// Tests behavior when the optimization guide service is disabled.
class BlinkOptimizationGuideDisabledBrowserTest
    : public BlinkOptimizationGuideBrowserTestBase {
 public:
  BlinkOptimizationGuideDisabledBrowserTest() {
    // Disable the optimization guide service.
    scoped_feature_list_.InitAndDisableFeature(features::kOptimizationHints);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BlinkOptimizationGuideDisabledBrowserTest,
                       OptimizationGuideIsDisabled) {
  // The optimization guide service shouldn't be available.
  EXPECT_FALSE(OptimizationGuideKeyedServiceFactory::GetForProfile(
      browser()->profile()));

  ui_test_utils::NavigateToURL(browser(), GetURLWithMockHost("/simple.html"));

  // Navigation started, but the web contents observer for the Blink
  // optimization guide shouldn't be created.
  EXPECT_FALSE(GetObserverForActiveWebContents());
}

}  // namespace optimization_guide
