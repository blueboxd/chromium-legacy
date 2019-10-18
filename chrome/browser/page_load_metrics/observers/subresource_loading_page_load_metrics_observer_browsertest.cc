// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/subresource_loading_page_load_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class SubresourceLoadingPageLoadMetricsObserverBrowserTest
    : public InProcessBrowserTest {
 public:
  SubresourceLoadingPageLoadMetricsObserverBrowserTest() = default;
  ~SubresourceLoadingPageLoadMetricsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/subresource_loading");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       BeforeFCPPlumbing) {
  GURL url = embedded_test_server()->GetURL("origin.com", "/index.html");

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 2,
      1);
}

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       HistoryPlumbing) {
  GURL url = embedded_test_server()->GetURL("origin.com", "/index.html");

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);

  // Revisit and expect a 0 days-ago entry.
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0, 1);
}
