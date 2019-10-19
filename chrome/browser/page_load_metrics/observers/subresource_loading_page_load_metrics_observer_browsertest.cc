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

  void NavigateToPath(const std::string& path) {
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("origin.com", path));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateAway() {
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
    base::RunLoop().RunUntilIdle();
  }
};

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       BeforeFCPPlumbing) {
  base::HistogramTester histogram_tester;
  NavigateToPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 2,
      1);
}

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       HistoryPlumbing) {
  base::HistogramTester histogram_tester;
  NavigateToPath("/index.html");
  NavigateAway();
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);

  // Revisit and expect a 0 days-ago entry.
  NavigateToPath("/index.html");
  NavigateAway();
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0, 1);
}

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_None) {
  base::HistogramTester histogram_tester;
  NavigateToPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);
}

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_CookiesOnNextPageLoad) {
  base::HistogramTester histogram_tester;
  NavigateToPath("/set_cookies.html");
  NavigateToPath("/index.html");
  NavigateAway();

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", true, 1);
  // From the first page load.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);
}

IN_PROC_BROWSER_TEST_F(SubresourceLoadingPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_CookiesOnRedirect) {
  base::HistogramTester histogram_tester;
  NavigateToPath("/set_cookies.html");
  NavigateToPath("/redirect_to_index.html");
  NavigateAway();

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", true, 1);
  // From the first page load.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", false, 1);
}
