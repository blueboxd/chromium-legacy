// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/navigation_predictor/navigation_predictor.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  base::RunLoop().RunUntilIdle();
  for (size_t attempt = 0; attempt < 50; ++attempt) {
    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets)
      total_count += bucket.count;
    if (total_count >= count)
      return;
    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramBucketUntilCountReached(
    base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    base::HistogramBase::Sample target_bucket,
    size_t count) {
  base::RunLoop().RunUntilIdle();
  for (size_t attempt = 0; attempt < 50; ++attempt) {
    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets) {
      if (bucket.min == target_bucket)
        total_count += bucket.count;
    }
    if (total_count >= count)
      return;
    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace

class NavigationPredictorBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  NavigationPredictorBrowserTest()
      : subresource_filter::SubresourceFilterBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kNavigationPredictor,
        {{"same_origin_preconnecting_allowed", "true"}});
  }

  void SetUp() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(https_server_->Start());

    http_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTP));
    http_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(http_server_->Start());

    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  const GURL GetHttpTestURL(const char* file) const {
    return http_server_->GetURL(file);
  }

  void WaitForLayout(base::HistogramTester* histogram_tester) {
    // Force a re-layout by adding a text node.
    EXPECT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "document.body.appendChild(document.createTextNode('node'))"));
    RetryForHistogramUntilCountReached(
        histogram_tester, "AnchorElementMetrics.Visible.RatioArea", 1);
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictorBrowserTest);
};

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, Pipeline) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 5, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
}

// Test that no metrics are recorded in off-the-record profiles.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, PipelineOffTheRecord) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  Browser* incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, url);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 0);
}

// Test that the browser does not process anchor element metrics from an http
// web page on page load.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, PipelineHttp) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetHttpTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 0);
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 0);
}

// Test that anchor elements within an iframe tagged as an ad are discarded when
// predicting next navigation.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, PipelineAdsFrameTagged) {
  // iframe_ads_simple_page_with_anchors.html is an iframe referenced by
  // page_with_ads_iframe.html.
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "iframe_ads_simple_page_with_anchors.html"));

  base::HistogramTester histogram_tester;

  GURL url = GetTestURL("/page_with_ads_iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "AnchorElementMetrics.IsAdFrameElement", 4);

  histogram_tester.ExpectTotalCount("AnchorElementMetrics.IsAdFrameElement", 7);
  histogram_tester.ExpectBucketCount("AnchorElementMetrics.IsAdFrameElement",
                                     0 /* false */, 5);
  histogram_tester.ExpectBucketCount("AnchorElementMetrics.IsAdFrameElement",
                                     1 /* true */, 2);
}

// Test that anchor elements within an iframe not tagged as ad are not discarded
// when predicting next navigation.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       PipelineAdsFrameNotTagged) {
  base::HistogramTester histogram_tester;

  GURL url = GetTestURL("/page_with_ads_iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 7, 1);
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "AnchorElementMetrics.IsAdFrameElement", 7);

  histogram_tester.ExpectUniqueSample("AnchorElementMetrics.IsAdFrameElement",
                                      0 /* false */, 7);
}

// Test that navigation score of anchor elements can be calculated on page load.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, NavigationScore) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Visible.HighestNavigationScore", 1);
}

// Simulate a click at the anchor element.
// Test that timing info (DurationLoadToFirstClick) can be recorded.
// And that the navigation score can be looked up.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, ClickAnchorElement) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.DurationLoadToFirstClick", 1);
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.NavigationScore", 1);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kNone, 1);
}

// Simulate a click at the anchor element.
// Test that the action accuracy is properly recorded.
// User clicks on an anchor element that points to a origin different than the
// origin of the URL prefetched.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ActionAccuracy_DifferentOrigin) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
      NavigationPredictor::ActionAccuracy::
          kPrefetchActionClickToDifferentOrigin,
      1);
}

// Disabled because it fails when SingleProcessMash feature is enabled. Since
// Navigation Predictor is not going to be enabled on Chrome OS, disabling the
// browser test on that platform is fine.
#if defined(OS_CHROMEOS)
#define DISABLE_ON_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_CHROMEOS(x) x
#endif

// Simulate a click at the anchor element.
// Test that the action accuracy is properly recorded.
// User clicks on an anchor element that points to a origin different than the
// origin of the URL prefetched.
IN_PROC_BROWSER_TEST_F(
    NavigationPredictorBrowserTest,
    DISABLE_ON_CHROMEOS(ActionAccuracy_DifferentOrigin_VisibilityChanged)) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
      NavigationPredictor::ActionAccuracy::
          kPrefetchActionClickToDifferentOrigin,
      1);

  // Change to visibile.
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();
  histogram_tester.ExpectTotalCount("NavigationPredictor.OnNonDSE.ActionTaken",
                                    1);

  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  histogram_tester.ExpectTotalCount("NavigationPredictor.OnNonDSE.ActionTaken",
                                    1);

  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();
  histogram_tester.ExpectTotalCount("NavigationPredictor.OnNonDSE.ActionTaken",
                                    2);
  histogram_tester.ExpectBucketCount(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPreconnectOnVisibilityChange, 1);

  // Hiding and showing the tab again should cause change in histograms since
  // Pre* on tab foreground is done more than once per page.
  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  histogram_tester.ExpectTotalCount("NavigationPredictor.OnNonDSE.ActionTaken",
                                    2);
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();
  histogram_tester.ExpectTotalCount("NavigationPredictor.OnNonDSE.ActionTaken",
                                    3);
}

// Test that we preconnect after the last preconnect timed out.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       DISABLE_ON_CHROMEOS(ActionAccuracy_timeout)) {
  base::HistogramTester histogram_tester;

  base::test::ScopedFeatureList scoped_feature_list;
  std::map<std::string, std::string> parameters;
  parameters["unused_idle_socket_timeout_seconds"] = "0";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kNetUnusedIdleSocketTimeout, parameters);

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  RetryForHistogramBucketUntilCountReached(
      &histogram_tester, "NavigationPredictor.OnNonDSE.ActionTaken",
      static_cast<base::HistogramBase::Sample>(
          NavigationPredictor::Action::kPreconnectAfterTimeout),
      1);

  EXPECT_LT(0, histogram_tester.GetBucketCount(
                   "NavigationPredictor.OnNonDSE.ActionTaken",
                   static_cast<base::HistogramBase::Sample>(
                       NavigationPredictor::Action::kPreconnectAfterTimeout)));
}

// Test that we don't preconnect after the last preconnect timed out when
// retry_preconnect_wait_time_ms is negative.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       DISABLE_ON_CHROMEOS(ActionAccuracy_timeout_no_retry)) {
  base::HistogramTester histogram_tester;

  base::test::ScopedFeatureList scoped_feature_list_net;
  // -1 would force synchronous retries if retries were not disabled.
  scoped_feature_list_net.InitAndEnableFeatureWithParameters(
      net::features::kNetUnusedIdleSocketTimeout,
      {{"unused_idle_socket_timeout_seconds", "-1"}});

  base::test::ScopedFeatureList scoped_feature_list_predictor;
  scoped_feature_list_predictor.InitAndEnableFeatureWithParameters(
      blink::features::kNavigationPredictor,
      {{"retry_preconnect_wait_time_ms", "-1"}});

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "NavigationPredictor.OnNonDSE.ActionTaken",
                   static_cast<base::HistogramBase::Sample>(
                       NavigationPredictor::Action::kPreconnectAfterTimeout)));
}

// Test that the action accuracy is properly recorded and when same origin
// preconnections are enabled, then navigation predictor initiates the
// preconnection.
IN_PROC_BROWSER_TEST_F(
    NavigationPredictorBrowserTest,
    DISABLE_ON_CHROMEOS(
        ActionAccuracy_DifferentOrigin_VisibilityChangedPreconnectEnabled)) {
  std::map<std::string, std::string> parameters;
  base::test::ScopedFeatureList feature_list;
  parameters["same_origin_preconnecting_allowed"] = "true";
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kNavigationPredictor, parameters);

  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);

  // Change to visible.
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();
  histogram_tester.ExpectTotalCount("NavigationPredictor.OnNonDSE.ActionTaken",
                                    1);

  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  histogram_tester.ExpectTotalCount("NavigationPredictor.OnNonDSE.ActionTaken",
                                    1);

  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();
  histogram_tester.ExpectTotalCount("NavigationPredictor.OnNonDSE.ActionTaken",
                                    2);
  histogram_tester.ExpectBucketCount(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPreconnectOnVisibilityChange, 1);

  // Hiding and showing the tab again should cause change in histograms since
  // Pre* on tab foreground is done more than once per page.
  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  histogram_tester.ExpectTotalCount("NavigationPredictor.OnNonDSE.ActionTaken",
                                    2);
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();
  histogram_tester.ExpectTotalCount("NavigationPredictor.OnNonDSE.ActionTaken",
                                    3);
  histogram_tester.ExpectBucketCount(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPreconnectOnVisibilityChange, 2);
}

IN_PROC_BROWSER_TEST_F(
    NavigationPredictorBrowserTest,
    DISABLE_ON_CHROMEOS(NoPreconnectNonSearchOnOtherHostLinks)) {
  std::map<std::string, std::string> parameters;
  base::test::ScopedFeatureList feature_list;
  parameters["same_origin_preconnecting_allowed"] = "true";
  parameters["preconnect_skip_link_scores"] = "false";
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kNavigationPredictor, parameters);

  base::HistogramTester histogram_tester;

  // This page only has non-same host links.
  const GURL& url = GetTestURL("/anchors_different_area.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kNone, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       DISABLE_ON_CHROMEOS(PreconnectNonSearch)) {
  std::map<std::string, std::string> parameters;
  base::test::ScopedFeatureList feature_list;
  parameters["same_origin_preconnecting_allowed"] = "true";
  parameters["preconnect_skip_link_scores"] = "true";
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kNavigationPredictor, parameters);

  base::HistogramTester histogram_tester;

  // This page only has non-same host links.
  const GURL& url = GetTestURL("/anchors_different_area.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPreconnect, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       DISABLE_ON_CHROMEOS(PrefetchAfterPreconnect)) {
  prerender::PrerenderManager::SetMode(
      prerender::PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH);

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder =
      std::make_unique<ukm::TestAutoSetUkmRecorder>();

  std::map<std::string, std::string> parameters;
  base::test::ScopedFeatureList feature_list;
  parameters["same_origin_preconnecting_allowed"] = "true";
  parameters["prefetch_after_preconnect"] = "true";
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kNavigationPredictor, parameters);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('example').click();"));
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
      NavigationPredictor::ActionAccuracy::kPrefetchActionClickToSameURL, 1);

  const auto& entries = ukm_recorder->GetMergedEntriesByName(
      ukm::builders::NoStatePrefetch::kEntryName);
  EXPECT_EQ(1u, entries.size());

  for (const auto& kv : entries) {
    EXPECT_TRUE(ukm_recorder->EntryHasMetric(
        kv.second.get(),
        ukm::builders::NoStatePrefetch::kPrefetchedRecently_FinalStatusName));
    EXPECT_TRUE(ukm_recorder->EntryHasMetric(
        kv.second.get(),
        ukm::builders::NoStatePrefetch::kPrefetchedRecently_OriginName));
    EXPECT_TRUE(ukm_recorder->EntryHasMetric(
        kv.second.get(),
        ukm::builders::NoStatePrefetch::kPrefetchedRecently_PrefetchAgeName));
  }
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       DISABLE_ON_CHROMEOS(NoPreconnectSearch)) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] =
      "/anchors_different_area.html?q={searchTerms}";

  // Force Preconnect on
  std::map<std::string, std::string> parameters;
  base::test::ScopedFeatureList feature_list;
  parameters["same_origin_preconnecting_allowed"] = "true";
  parameters["preconnect_skip_link_scores"] = "true";
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kNavigationPredictor, parameters);

  // Set up default search engine.
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  base::HistogramTester histogram_tester;

  // This page only has non-same host links.
  const GURL& url = GetTestURL("/anchors_different_area.html?q=cats");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectUniqueSample("NavigationPredictor.OnDSE.ActionTaken",
                                      NavigationPredictor::Action::kNone, 1);
}

// Simulate a click at the anchor element.
// Test that the action accuracy is properly recorded.
// User clicks on an anchor element that points to same URL as the URL
// prefetched.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ActionAccuracy_SameOrigin) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('example').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 1);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken",
      NavigationPredictor::ActionAccuracy::kPrefetchActionClickToSameURL, 1);
}

// Simulate a click at the anchor element in off-the-record profile. Metrics
// should not be recorded.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ClickAnchorElementOffTheRecord) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");

  Browser* incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, url);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(content::ExecuteScript(
      incognito->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.HrefEngagementScore2", 0);
}

// Simulate click at the anchor element.
// Test that correct area ranks are recorded.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, AreaRank) {
  base::HistogramTester histogram_tester;

  // This test file contains 5 anchors with different size.
  const GURL& url = GetTestURL("/anchors_different_area.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('medium').click();"));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample("AnchorElementMetrics.Clicked.AreaRank",
                                      2, 1);
  histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                    5);
}

// Test that MergeMetricsSameTargetUrl merges anchor elements having the same
// href. The html file contains two anchor elements having the same href.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       MergeMetricsSameTargetUrl_ClickHrefWithNoMergedImage) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                    1);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('diffHref').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'diffHref' points to an href. No image in the
  // webpage also points to an image. So, clicking on this non-image anchor
  // element, should not be recorded as "ContainsImage".
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 0);
}

// Test that MergeMetricsSameTargetUrl merges anchor elements having the same
// href. The html file contains two anchor elements having the same href.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       MergeMetricsSameTargetUrl_ClickHrefWithMergedImage) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectTotalCount("AnchorElementMetrics.Visible.RatioArea",
                                    1);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'google' points to an href. Another image in the
  // webpage also points to an image. So, even though we clicked on a non-image
  // anchor element, it should be recorded as "ContainsImage".
    histogram_tester.ExpectTotalCount(
        "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 1);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       AnchorElementClickedOnSearchEnginePage) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] = "/anchors_same_href.html?q={searchTerms}";

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html?q=cats");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'google' points to an href that's on a different
  // host.
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 1);
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Clicked.OnDSE.SameHost", 0, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       AnchorElementClickedOnNonSearchEnginePage) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] = "/somne_other_url.html?q={searchTerms}";

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/anchors_same_href.html?q=cats");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  // Anchor element with id 'google' points to an href that's on a different
  // host.
  histogram_tester.ExpectTotalCount(
      "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage", 1);
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Clicked.OnNonDSE.SameHost", 0, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ActionPrefetch_NoSameHostAnchorElement) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 5, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kNone, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ActionPrefetch_SameHostAnchorElement) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.OnNonDSE.ActionTaken",
      NavigationPredictor::Action::kPrefetch, 1);
}

// Tests that the browser only receives anchor elements that are in the
// viewport, and from anchor elements whose target differ from document URL
// by one digit.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ViewportOnlyAndUrlIncrementByOne) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/long_page_with_anchors-1.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WaitForLayout(&histogram_tester);

  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", 2, 1);
  // Same document anchor element should be removed after merge.
  histogram_tester.ExpectUniqueSample(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 2, 1);
}
