// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/observers/back_forward_cache_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class BackForwardCachePageLoadMetricsObserverBrowserTest
    : public InProcessBrowserTest {
 public:
  ~BackForwardCachePageLoadMetricsObserverBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          {{"TimeToLiveInBackForwardCacheInSeconds", "3600"}}}},
        {});

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* top_frame_host() {
    return web_contents()->GetMainFrame();
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents());
  }

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

}  // namespace

#if defined(OS_MACOSX)
#define MAYBE_FirstPaintAfterBackForwardCacheRestore DISABLED_FirstPaintAfterBackForwardCacheRestore
#else
#define MAYBE_FirstPaintAfterBackForwardCacheRestore FirstPaintAfterBackForwardCacheRestore
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCachePageLoadMetricsObserverBrowserTest,
                       MAYBE_FirstPaintAfterBackForwardCacheRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHost* rfh_a = top_frame_host();

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_EQ(rfh_a, top_frame_host());
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());

    waiter->Wait();
    histogram_tester_.ExpectTotalCount(
        internal::kHistogramFirstPaintAfterBackForwardCacheRestore, 1);
  }

  // The render frame host for the page B was likely in the back-forward cache
  // just after the history navigation, but now this might be evicted due to
  // outstanding-network request.

  // Navigate to B again.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back to A again.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_EQ(rfh_a, top_frame_host());
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());

    waiter->Wait();
    histogram_tester_.ExpectTotalCount(
        internal::kHistogramFirstPaintAfterBackForwardCacheRestore, 2);
  }
}

IN_PROC_BROWSER_TEST_F(BackForwardCachePageLoadMetricsObserverBrowserTest,
                       FirstPaintAfterBackForwardCacheRestoreBackground) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHost* rfh_a = top_frame_host();

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstPaintAfterBackForwardCacheRestore);

    web_contents()->GetController().GoBack();

    // Make the tab backgrounded before the tab goes back.
    web_contents()->WasHidden();

    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_EQ(rfh_a, top_frame_host());
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());

    web_contents()->WasShown();

    waiter->Wait();

    // As the tab goes to the background before the first paint, the UMA is not
    // recorded.
    histogram_tester_.ExpectTotalCount(
        internal::kHistogramFirstPaintAfterBackForwardCacheRestore, 0);
  }
}

IN_PROC_BROWSER_TEST_F(BackForwardCachePageLoadMetricsObserverBrowserTest,
                       FirstInputDelayAfterBackForwardCacheRestoreBackground) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHost* rfh_a = top_frame_host();

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramFirstInputDelayAfterBackForwardCacheRestore, 0);

  // Go back to A.
  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstInputDelayAfterBackForwardCacheRestore);

    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    EXPECT_EQ(rfh_a, top_frame_host());
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());

    content::SimulateMouseClick(web_contents(), 0,
                                blink::WebPointerProperties::Button::kLeft);

    waiter->Wait();

    histogram_tester_.ExpectTotalCount(
        internal::kHistogramFirstInputDelayAfterBackForwardCacheRestore, 1);
  }
}
