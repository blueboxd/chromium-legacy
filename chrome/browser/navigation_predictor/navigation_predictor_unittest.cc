// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include <map>
#include <string>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"
#include "url/gurl.h"

namespace {

class TestNavigationPredictor : public NavigationPredictor {
 public:
  explicit TestNavigationPredictor(
      mojo::InterfaceRequest<AnchorElementMetricsHost> request,
      content::RenderFrameHost* render_frame_host,
      bool init_feature_list)
      : NavigationPredictor(render_frame_host), binding_(this) {
    binding_.Bind(std::move(request));
    if (init_feature_list) {
      const std::vector<base::Feature> features = {
          blink::features::kNavigationPredictor};
      feature_list_.InitWithFeatures(features, {});
    }
  }

  ~TestNavigationPredictor() override {}

  base::Optional<GURL> prefetch_url() const { return prefetch_url_; }

  base::Optional<url::Origin> preconnect_origin() const {
    return preconnect_origin_;
  }

  const std::map<GURL, int>& GetAreaRankMap() const { return area_rank_map_; }

  bool prefetch_url_prefetched() const { return prefetch_url_prefetched_; }

  int calls_to_prefetch() const { return calls_to_prefetch_; }

 private:
  double CalculateAnchorNavigationScore(
      const blink::mojom::AnchorElementMetrics& metrics,
      double document_engagement_score,
      double target_engagement_score,
      int area_rank) const override {
    area_rank_map_.emplace(std::make_pair(metrics.target_url, area_rank));
    return 100 * metrics.ratio_area;
  }

  void Prefetch(prerender::PrerenderManager* prerender_manager) override {
    calls_to_prefetch_ += 1;
  }

  // Maps from target URL to area rank of the anchor element.
  mutable std::map<GURL, int> area_rank_map_;

  base::test::ScopedFeatureList feature_list_;

  // Used to bind Mojo interface
  mojo::Binding<AnchorElementMetricsHost> binding_;

  int calls_to_prefetch_ = 0;
};

class NavigationPredictorTest : public ChromeRenderViewHostTestHarness {
 public:
  NavigationPredictorTest() = default;
  ~NavigationPredictorTest() override = default;

  // Helper function to generate mojom metrics.
  blink::mojom::AnchorElementMetricsPtr CreateMetricsPtr(
      const std::string& source_url,
      const std::string& target_url,
      float ratio_area) const {
    auto metrics = blink::mojom::AnchorElementMetrics::New();
    metrics->source_url = GURL(source_url);
    metrics->target_url = GURL(target_url);
    metrics->ratio_area = ratio_area;
    return metrics;
  }

  gfx::Size GetDefaultViewport() { return gfx::Size(600, 800); }

  blink::mojom::AnchorElementMetricsHost* predictor_service() const {
    return predictor_service_.get();
  }

  TestNavigationPredictor* predictor_service_helper() const {
    return predictor_service_helper_.get();
  }

  base::Optional<GURL> prefetch_url() const {
    return predictor_service_helper_->prefetch_url();
  }

  base::Optional<url::Origin> preconnect_origin() const {
    return predictor_service_helper_->preconnect_origin();
  }

  bool prefetch_url_prefetched() {
    return predictor_service_helper_->prefetch_url_prefetched();
  }

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    predictor_service_helper_ = std::make_unique<TestNavigationPredictor>(
        mojo::MakeRequest(&predictor_service_), main_rfh(),
        !field_trial_initiated_);
  }

  void SetupFieldTrial(base::Optional<int> preconnect_origin_score_threshold,
                       base::Optional<int> prefetch_url_score_threshold,
                       base::Optional<bool> prefetch_after_preconnect) {
    if (field_trial_initiated_)
      return;

    field_trial_initiated_ = true;
    const std::string kTrialName = "TrialFoo2";
    const std::string kGroupName = "GroupFoo2";  // Value not used

    std::map<std::string, std::string> params;
    if (preconnect_origin_score_threshold.has_value()) {
      params["preconnect_origin_score_threshold"] =
          base::NumberToString(preconnect_origin_score_threshold.value());
    }
    if (prefetch_url_score_threshold.has_value()) {
      params["prefetch_url_score_threshold"] =
          base::NumberToString(prefetch_url_score_threshold.value());
    }
    if (prefetch_after_preconnect.has_value()) {
      params["prefetch_after_preconnect"] =
          prefetch_after_preconnect.value() ? "true" : "false";
    }
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        blink::features::kNavigationPredictor, params);
  }

  blink::mojom::AnchorElementMetricsHostPtr predictor_service_;
  std::unique_ptr<TestNavigationPredictor> predictor_service_helper_;

 private:
  base::test::ScopedFeatureList scoped_feature_list;

  bool field_trial_initiated_ = false;
};

}  // namespace

// Basic test to check the ReportAnchorElementMetricsOnClick method can be
// called.
TEST_F(NavigationPredictorTest, ReportAnchorElementMetricsOnClick) {
  base::HistogramTester histogram_tester;

  auto metrics =
      CreateMetricsPtr("https://example.com", "https://google.com", 0.1);
  predictor_service()->ReportAnchorElementMetricsOnClick(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
      NavigationPredictor::ActionAccuracy::kNoActionTakenClickHappened, 1);
}

// Test that ReportAnchorElementMetricsOnLoad method can be called.
TEST_F(NavigationPredictorTest, ReportAnchorElementMetricsOnLoad) {
  base::HistogramTester histogram_tester;

  auto metrics =
      CreateMetricsPtr("https://example.com", "https://google.com", 0.1);
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics_vector;
  metrics_vector.push_back(std::move(metrics));
  predictor_service()->ReportAnchorElementMetricsOnLoad(
      std::move(metrics_vector), GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 1);
}

// Test that if source/target url is not http or https, no score will be
// calculated.
TEST_F(NavigationPredictorTest,
       BadUrlReportAnchorElementMetricsOnClick_ftp_src) {
  base::HistogramTester histogram_tester;

  auto metrics =
      CreateMetricsPtr("ftp://example.com", "https://google.com", 0.1);
  predictor_service()->ReportAnchorElementMetricsOnClick(std::move(metrics));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 0);
}

// Test that if source/target url is not http or https, no navigation score will
// be calculated.
TEST_F(NavigationPredictorTest,
       BadUrlReportAnchorElementMetricsOnLoad_ftp_target) {
  base::HistogramTester histogram_tester;

  auto metrics =
      CreateMetricsPtr("https://example.com", "ftp://google.com", 0.1);
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics_vector;
  metrics_vector.push_back(std::move(metrics));
  predictor_service()->ReportAnchorElementMetricsOnLoad(
      std::move(metrics_vector), GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 0);
}

// Test that if the target url is not https, no navigation score will
// be calculated.
TEST_F(NavigationPredictorTest,
       BadUrlReportAnchorElementMetricsOnLoad_http_target) {
  base::HistogramTester histogram_tester;

  auto metrics =
      CreateMetricsPtr("https://example.com", "http://google.com", 0.1);
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics_vector;
  metrics_vector.push_back(std::move(metrics));
  predictor_service()->ReportAnchorElementMetricsOnLoad(
      std::move(metrics_vector), GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 0);
}

// Test that if the source url is not https, no navigation score will
// be calculated.
TEST_F(NavigationPredictorTest,
       BadUrlReportAnchorElementMetricsOnLoad_http_src) {
  base::HistogramTester histogram_tester;

  auto metrics =
      CreateMetricsPtr("http://example.com", "https://google.com", 0.1);
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics_vector;
  metrics_vector.push_back(std::move(metrics));
  predictor_service()->ReportAnchorElementMetricsOnLoad(
      std::move(metrics_vector), GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 0);
}

TEST_F(NavigationPredictorTest, Merge_UniqueAnchorElements) {
  base::HistogramTester histogram_tester;

  const std::string source = "https://example.com";
  const std::string href_xlarge = "https://example.com/xlarge";
  const std::string href_large = "https://google.com/large";
  const std::string href_medium = "https://google.com/medium";
  const std::string href_small = "https://google.com/small";
  const std::string href_xsmall = "https://google.com/xsmall";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, href_xsmall, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_large, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_xlarge, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_small, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_medium, 0.01));

  int number_of_metrics_sent = metrics.size();
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements",
      number_of_metrics_sent, 1);
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge",
      number_of_metrics_sent, 1);
}

TEST_F(NavigationPredictorTest, Merge_DuplicateAnchorElements) {
  base::HistogramTester histogram_tester;

  const std::string source = "https://example.com";
  const std::string href = "https://example.com/xlarge";
  // URLs that differ only in ref fragments should be merged.
  const std::string href_ref_1 = "https://example.com/xlarge#";
  const std::string href_ref_2 = "https://example.com/xlarge#ref_foo";
  const std::string href_query_1 = "https://example.com/xlarge?q=foo";
  const std::string href_query_ref = "https://example.com/xlarge?q=foo#ref_bar";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, href, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_ref_1, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_ref_2, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_query_1, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_query_ref, 0.01));

  int number_of_metrics_sent = metrics.size();
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements",
      number_of_metrics_sent, 1);

  // Only two anchor elements are unique: |href| and |href_query_1|.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
}

TEST_F(NavigationPredictorTest, Merge_AnchorElementSameAsDocumentURL) {
  base::HistogramTester histogram_tester;

  const std::string source = "https://example.com/";
  const std::string href = "https://example.com/";
  // URLs that differ only in ref fragments should be merged.
  const std::string href_ref_1 = "https://example.com/#ref=foo";
  const std::string href_ref_2 = "https://example.com/xlarge#ref=foo";
  const std::string href_query_1 = "https://example.com/xlarge?q=foo";
  const std::string href_query_ref = "https://example.com/xlarge?q=foo#ref_bar";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, href, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_ref_1, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_ref_2, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_query_1, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_query_ref, 0.01));

  int number_of_metrics_sent = metrics.size();
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements",
      number_of_metrics_sent, 1);
  // Only two anchor elements are unique and different from the document URL:
  // |href_ref_2| and |href_query_1|.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
}

// In this test, multiple anchor element metrics are sent to
// ReportAnchorElementMetricsOnLoad. Test that CalculateAnchorNavigationScore
// works, and that highest navigation score can be recorded correctly.
TEST_F(NavigationPredictorTest, MultipleAnchorElementMetricsOnLoad) {
  base::HistogramTester histogram_tester;

  const std::string source = "https://example.com";
  const std::string href_xlarge = "https://example.com/xlarge";
  const std::string href_large = "https://google.com/large";
  const std::string href_medium = "https://google.com/medium";
  const std::string href_small = "https://google.com/small";
  const std::string href_xsmall = "https://google.com/xsmall";
  const std::string http_href_xsmall = "http://google.com/xsmall";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, href_xsmall, 0.01));
  metrics.push_back(CreateMetricsPtr(source, http_href_xsmall, 0.01));
  metrics.push_back(CreateMetricsPtr(source, href_large, 0.08));
  metrics.push_back(CreateMetricsPtr(source, href_xlarge, 0.1));
  metrics.push_back(CreateMetricsPtr(source, href_small, 0.02));
  metrics.push_back(CreateMetricsPtr(source, href_medium, 0.05));

  int number_of_metrics_sent = metrics.size();
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  const std::map<GURL, int>& area_rank_map =
      predictor_service_helper()->GetAreaRankMap();
  // Exclude the http anchor element from |number_of_metrics_sent|.
  EXPECT_EQ(number_of_metrics_sent - 1, static_cast<int>(area_rank_map.size()));
  EXPECT_EQ(0, area_rank_map.find(GURL(href_xlarge))->second);
  EXPECT_EQ(1, area_rank_map.find(GURL(href_large))->second);
  EXPECT_EQ(2, area_rank_map.find(GURL(href_medium))->second);
  EXPECT_EQ(3, area_rank_map.find(GURL(href_small))->second);
  EXPECT_EQ(4, area_rank_map.find(GURL(href_xsmall))->second);
  EXPECT_EQ(area_rank_map.end(), area_rank_map.find(GURL(http_href_xsmall)));

  // The highest score is 100 (scale factor) * 0.1 (largest area) = 10.
  // After scaling the navigation score across all anchor elements, the score
  // becomes 38.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 38, 1);
  histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                    5);
}

TEST_F(NavigationPredictorTest, ActionTaken_NoSameHost_Prefetch) {
  const std::string source = "https://example.com";
  const std::string href_xlarge = "https://example2.com/xlarge";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, href_xlarge, 0.1));

  base::HistogramTester histogram_tester;
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kNone, 1);
  EXPECT_FALSE(prefetch_url().has_value());
}

// URL with highest prefetch score is from the same origin. Prefetch is done.
TEST_F(NavigationPredictorTest, ActionTaken_SameOrigin_Prefetch) {
  const std::string source = "https://example.com";
  const std::string same_origin_href_small = "https://example.com/small";
  const std::string same_origin_href_large = "https://example.com/large";
  const std::string diff_origin_href_xsmall = "https://example2.com/xsmall";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_large, 1));
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_small, 0.01));
  metrics.push_back(CreateMetricsPtr(source, diff_origin_href_xsmall, 0.01));

  base::HistogramTester histogram_tester;
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);
  EXPECT_EQ(GURL(same_origin_href_large), prefetch_url());

  auto metrics_clicked = CreateMetricsPtr(source, same_origin_href_small, 0.01);
  predictor_service()->ReportAnchorElementMetricsOnClick(
      std::move(metrics_clicked));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
      NavigationPredictor::ActionAccuracy::kPrefetchActionClickToSameOrigin, 1);
}

// URL with highest prefetch score is from a different origin. So, prefetch is
// not done.
TEST_F(NavigationPredictorTest, ActionTaken_SameOrigin_Prefetch_NotSameOrigin) {
  const std::string source = "https://example.com";
  const std::string same_origin_href_small = "https://example.com/small";
  const std::string same_origin_href_large = "https://example.com/large";
  const std::string diff_origin_href_xlarge = "https://example2.com/xlarge";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_large, 1));
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_small, 0.01));
  metrics.push_back(CreateMetricsPtr(source, diff_origin_href_xlarge, 10));

  base::HistogramTester histogram_tester;
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kNone, 1);
  EXPECT_FALSE(prefetch_url().has_value());

  auto metrics_clicked = CreateMetricsPtr(source, same_origin_href_small, 0.01);
  predictor_service()->ReportAnchorElementMetricsOnClick(
      std::move(metrics_clicked));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
      NavigationPredictor::ActionAccuracy::kNoActionTakenClickHappened, 1);
}

TEST_F(NavigationPredictorTest,
       ActionTaken_SameOrigin_DifferentScheme_Prefetch) {
  const std::string source = "https://example.com";
  const std::string same_origin_href_small = "http://example.com/small";
  const std::string diff_origin_href_xlarge = "https://example2.com/xlarge";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_small, 0.01));
  metrics.push_back(CreateMetricsPtr(source, diff_origin_href_xlarge, 1));

  base::HistogramTester histogram_tester;
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kNone, 1);
  EXPECT_FALSE(prefetch_url().has_value());
}

class NavigationPredictorPrefetchAfterPreconnectEnabledTest
    : public NavigationPredictorTest {
 public:
  NavigationPredictorPrefetchAfterPreconnectEnabledTest() {
    SetupFieldTrial(base::nullopt, base::nullopt, true);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    predictor_service_helper_ = std::make_unique<TestNavigationPredictor>(
        mojo::MakeRequest(&predictor_service_), main_rfh(), false);
  }
};

// Test that a prefetch after preconnect occurs only when the current tab is
// in the foreground, and that it does not occur multiple times for the same
// URL.
TEST_F(NavigationPredictorPrefetchAfterPreconnectEnabledTest,
       PrefetchWithTabs) {
  const std::string source = "https://example.com";
  const std::string same_origin_href_large = "https://example.com/large";
  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_large, 1));

  // Hide the tab and load the page. The URL should not be prefetched.
  web_contents()->WasHidden();
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(prefetch_url_prefetched());
  EXPECT_EQ(predictor_service_helper_->calls_to_prefetch(), 0);

  // Making the tab visible should start a prefetch.
  web_contents()->WasShown();
  EXPECT_TRUE(prefetch_url_prefetched());
  EXPECT_EQ(predictor_service_helper_->calls_to_prefetch(), 1);

  // Switching a tab from HIDDEN to VISIBLE should not start a prefetch
  // if a prefetch already occurred for that URL.
  web_contents()->WasHidden();
  web_contents()->WasShown();
  EXPECT_TRUE(prefetch_url_prefetched());
  EXPECT_EQ(predictor_service_helper_->calls_to_prefetch(), 1);
}

// Framework for testing cases where prefetch is effectively
// disabled by setting |prefetch_url_score_threshold| to too high.
class NavigationPredictorPrefetchDisabledTest : public NavigationPredictorTest {
 public:
  NavigationPredictorPrefetchDisabledTest() {
    SetupFieldTrial(0 /* preconnect_origin_score_threshold */,
                    101 /* prefetch_url_score_threshold */, base::nullopt);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    predictor_service_helper_ = std::make_unique<TestNavigationPredictor>(
        mojo::MakeRequest(&predictor_service_), main_rfh(), false);
  }
};

// Disables prefetch and loads a page where the preconnect score of the document
// origin is highest among all origins. Verifies that navigation predictor
// preconnects to the document origin.
TEST_F(NavigationPredictorPrefetchDisabledTest,
       ActionTaken_SameOrigin_Prefetch_BelowThreshold) {
  const std::string source = "https://example.com";
  const std::string same_origin_href_small = "https://example.com/small";
  const std::string same_origin_href_large = "https://example.com/large";

  // Cross origin anchor element is small. This should result in example.com to
  // have the highest preconnect score.
  const std::string diff_origin_href_xsmall = "https://example2.com/xsmall";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_large, 1));
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_small, 0.01));
  metrics.push_back(CreateMetricsPtr(source, diff_origin_href_xsmall, 0.0001));

  base::HistogramTester histogram_tester;
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPreconnect, 1);
  EXPECT_FALSE(prefetch_url().has_value());
  EXPECT_EQ(url::Origin::Create(GURL(source)), preconnect_origin());

  auto metrics_clicked = CreateMetricsPtr(source, same_origin_href_small, 0.01);
  predictor_service()->ReportAnchorElementMetricsOnClick(
      std::move(metrics_clicked));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
      NavigationPredictor::ActionAccuracy::kPreconnectActionClickToSameOrigin,
      1);
}

// Disables prefetch and loads a page where the preconnect score of a cross
// origin is highest among all origins. Verifies that navigation predictor does
// not preconnect to the cross origin.
TEST_F(NavigationPredictorPrefetchDisabledTest,
       ActionTaken_PreconnectHighScoreIsCrossOrigin) {
  const std::string source = "https://example.com";
  const std::string same_origin_href_small = "https://example.com/small";
  const std::string same_origin_href_large = "https://example.com/large";

  // Cross origin anchor element is large. This should result in example2.com to
  // have the highest preconnect score.
  const std::string diff_origin_href_xlarge = "https://example2.com/xlarge";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_large, 1));
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_small, 0.01));
  metrics.push_back(CreateMetricsPtr(source, diff_origin_href_xlarge, 10));

  base::HistogramTester histogram_tester;
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kNone, 1);
  EXPECT_FALSE(prefetch_url().has_value());
  EXPECT_FALSE(preconnect_origin().has_value());

  auto metrics_clicked = CreateMetricsPtr(source, same_origin_href_small, 0.01);
  predictor_service()->ReportAnchorElementMetricsOnClick(
      std::move(metrics_clicked));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
      NavigationPredictor::ActionAccuracy::kNoActionTakenClickHappened, 1);
}

// Framework for testing cases where preconnect and prefetch are effectively
// disabled by setting their thresholds as too high.
class NavigationPredictorPreconnectPrefetchDisabledTest
    : public NavigationPredictorTest {
 public:
  NavigationPredictorPreconnectPrefetchDisabledTest() {
    SetupFieldTrial(101 /* preconnect_origin_score_threshold */,
                    101 /* prefetch_url_score_threshold */, base::nullopt);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    predictor_service_helper_ = std::make_unique<TestNavigationPredictor>(
        mojo::MakeRequest(&predictor_service_), main_rfh(), false);
  }
};

// No action should be taken when both preconnect and prefetch are effectively
// disabled.
TEST_F(NavigationPredictorPreconnectPrefetchDisabledTest,
       ActionTaken_SameOrigin_Prefetch_BelowThreshold) {
  const std::string source = "https://example.com";
  const std::string same_origin_href_small = "https://example.com/small";
  const std::string same_origin_href_large = "https://example.com/large";
  const std::string diff_origin_href_xsmall = "https://example2.com/xsmall";

  std::vector<blink::mojom::AnchorElementMetricsPtr> metrics;
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_large, 1));
  metrics.push_back(CreateMetricsPtr(source, same_origin_href_small, 0.01));
  metrics.push_back(CreateMetricsPtr(source, diff_origin_href_xsmall, 0.0001));

  base::HistogramTester histogram_tester;
  predictor_service()->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                        GetDefaultViewport());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kNone, 1);
  EXPECT_FALSE(prefetch_url().has_value());

  auto metrics_clicked = CreateMetricsPtr(source, same_origin_href_small, 0.01);
  predictor_service()->ReportAnchorElementMetricsOnClick(
      std::move(metrics_clicked));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
      NavigationPredictor::ActionAccuracy::kNoActionTakenClickHappened, 1);
}
