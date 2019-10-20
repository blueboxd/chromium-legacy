// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class NavigationPredictorPreconnectClientBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  NavigationPredictorPreconnectClientBrowserTest()
      : subresource_filter::SubresourceFilterBrowserTest() {
    feature_list_.InitFromCommandLine(std::string(),
                                      "NavigationPredictorPreconnectHoldback");
  }

  void SetUp() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(https_server_->Start());

    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  // Retries fetching |histogram_name| until it contains at least |count|
  // samples.
  void RetryForHistogramUntilCountReached(
      const base::HistogramTester& histogram_tester,
      const std::string& histogram_name,
      size_t count) {
    base::RunLoop().RunUntilIdle();
    for (size_t attempt = 0; attempt < 50; ++attempt) {
      const std::vector<base::Bucket> buckets =
          histogram_tester.GetAllSamples(histogram_name);
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

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictorPreconnectClientBrowserTest);
};

IN_PROC_BROWSER_TEST_F(NavigationPredictorPreconnectClientBrowserTest,
                       NoPreconnectSearch) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] =
      "/anchors_different_area.html?q={searchTerms}";
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
  const GURL& url = GetTestURL("/anchors_different_area.html?q=cats");

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);
  // There should be preconnect from navigation, but not preconnect client.
  histogram_tester.ExpectTotalCount("LoadingPredictor.PreconnectCount", 1);

  ui_test_utils::NavigateToURL(browser(), url);
  // There should be 2 preconnects from navigation, but not any from preconnect
  // client.
  histogram_tester.ExpectTotalCount("LoadingPredictor.PreconnectCount", 2);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorPreconnectClientBrowserTest,
                       PreconnectNotSearch) {
  const GURL& url = GetTestURL("/anchors_different_area.html");

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);
  // There should be preconnect from navigation and one from preconnect client.
  RetryForHistogramUntilCountReached(histogram_tester,
                                     "LoadingPredictor.PreconnectCount", 2);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorPreconnectClientBrowserTest,
                       PreconnectNotSearchBackgroundForeground) {
  const GURL& url = GetTestURL("/anchors_different_area.html");

  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);

  // There should be a navigational preconnect.
  histogram_tester.ExpectTotalCount("LoadingPredictor.PreconnectCount", 1);

  // Change to visible.
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();

  // After showing the contents, there should be a preconnect client preconnect.
  RetryForHistogramUntilCountReached(histogram_tester,
                                     "LoadingPredictor.PreconnectCount", 2);

  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();

  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();
  // After showing the contents again, there should be another preconnect client
  // preconnect.
  RetryForHistogramUntilCountReached(histogram_tester,
                                     "LoadingPredictor.PreconnectCount", 3);
}

class NavigationPredictorPreconnectClientBrowserTestWithUnusedIdleSocketTimeout
    : public NavigationPredictorPreconnectClientBrowserTest {
 public:
  NavigationPredictorPreconnectClientBrowserTestWithUnusedIdleSocketTimeout()
      : NavigationPredictorPreconnectClientBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kNetUnusedIdleSocketTimeout,
        {{"unused_idle_socket_timeout_seconds", "0"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that we preconnect after the last preconnect timed out.
IN_PROC_BROWSER_TEST_F(
    NavigationPredictorPreconnectClientBrowserTestWithUnusedIdleSocketTimeout,
    ActionAccuracy_timeout) {
  base::HistogramTester histogram_tester;

  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ui_test_utils::NavigateToURL(browser(), url);

  histogram_tester.ExpectTotalCount("LoadingPredictor.PreconnectCount", 1);
}

class NavigationPredictorPreconnectClientBrowserTestWithHoldback
    : public NavigationPredictorPreconnectClientBrowserTest {
 public:
  NavigationPredictorPreconnectClientBrowserTestWithHoldback()
      : NavigationPredictorPreconnectClientBrowserTest() {
    feature_list_.InitFromCommandLine("NavigationPredictorPreconnectHoldback",
                                      std::string());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    NavigationPredictorPreconnectClientBrowserTestWithHoldback,
    NoPreconnectHoldback) {
  const GURL& url = GetTestURL("/anchors_different_area.html");

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);
  // There should be preconnect from navigation, but not preconnect client.
  histogram_tester.ExpectTotalCount("LoadingPredictor.PreconnectCount", 1);

  ui_test_utils::NavigateToURL(browser(), url);
  // There should be 2 preconnects from navigation, but not any from preconnect
  // client.
  histogram_tester.ExpectTotalCount("LoadingPredictor.PreconnectCount", 2);
}

}  // namespace
