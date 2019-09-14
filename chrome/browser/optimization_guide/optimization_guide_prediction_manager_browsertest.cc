// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_test_waiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

class OptimizationGuidePredictionManagerBrowserTest
    : public InProcessBrowserTest {
 public:
  OptimizationGuidePredictionManagerBrowserTest() = default;
  ~OptimizationGuidePredictionManagerBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints,
         optimization_guide::features::kOptimizationGuideKeyedService},
        {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
    https_url_with_content_ = https_server_->GetURL("/english_page.html");
    https_url_without_content_ = https_server_->GetURL("/empty.html");
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void RegisterWithKeyedService() {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  GURL https_url_with_content() { return https_url_with_content_; }
  GURL https_url_without_content() { return https_url_without_content_; }

 private:
  GURL https_url_with_content_, https_url_without_content_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuidePredictionManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(OptimizationGuidePredictionManagerBrowserTest,
                       FCPReachedSessionStatisticsUpdated) {
  OptimizationGuideKeyedService* keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());

  RegisterWithKeyedService();
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  waiter->Wait();

  OptimizationGuideSessionStatistic* session_fcp =
      keyed_service->GetSessionFCPForTesting();
  EXPECT_TRUE(session_fcp);
  EXPECT_EQ(1u, session_fcp->GetNumberOfSamples());
}

IN_PROC_BROWSER_TEST_F(OptimizationGuidePredictionManagerBrowserTest,
                       NoFCPSessionStatisticsUnchanged) {
  OptimizationGuideKeyedService* keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());

  RegisterWithKeyedService();
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstPaint);
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  waiter->Wait();

  OptimizationGuideSessionStatistic* session_fcp =
      keyed_service->GetSessionFCPForTesting();
  float current_mean = session_fcp->GetMean();

  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kFirstLayout);
  ui_test_utils::NavigateToURL(browser(), https_url_without_content());
  waiter->Wait();
  EXPECT_EQ(1u, session_fcp->GetNumberOfSamples());
  EXPECT_EQ(current_mean, session_fcp->GetMean());
}
