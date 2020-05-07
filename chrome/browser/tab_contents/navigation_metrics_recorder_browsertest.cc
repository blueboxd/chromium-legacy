// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace {

typedef InProcessBrowserTest NavigationMetricsRecorderBrowserTest;

// A site engagement score that falls into the range for HIGH engagement level.
const int kHighEngagementScore = 50;

IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderBrowserTest, TestMetrics) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          web_contents);
  ASSERT_TRUE(recorder);

  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(),
                               GURL("data:text/html, <html></html>"));
  histograms.ExpectTotalCount("Navigation.MainFrameScheme", 1);
  histograms.ExpectBucketCount("Navigation.MainFrameScheme", 5 /* data: */, 1);
  histograms.ExpectTotalCount("Navigation.MainFrameSchemeDifferentPage", 1);
  histograms.ExpectBucketCount("Navigation.MainFrameSchemeDifferentPage",
                               5 /* data: */, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderBrowserTest,
                       Navigation_EngagementLevel) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          web_contents);
  ASSERT_TRUE(recorder);

  const GURL url("https://google.com");
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), url);
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 1);
  histograms.ExpectBucketCount("Navigation.MainFrame.SiteEngagementLevel",
                               blink::mojom::EngagementLevel::NONE, 1);

  SiteEngagementService::Get(browser()->profile())
      ->ResetBaseScoreForURL(url, kHighEngagementScore);
  ui_test_utils::NavigateToURL(browser(), url);
  histograms.ExpectTotalCount("Navigation.MainFrame.SiteEngagementLevel", 2);
  histograms.ExpectBucketCount("Navigation.MainFrame.SiteEngagementLevel",
                               blink::mojom::EngagementLevel::NONE, 1);
  histograms.ExpectBucketCount("Navigation.MainFrame.SiteEngagementLevel",
                               blink::mojom::EngagementLevel::HIGH, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationMetricsRecorderBrowserTest,
                       FormSubmission_EngagementLevel) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/form.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Submit a form and check the histograms. Before doing so, we set a high site
  // engagement score so that a single form submission doesn't affect the score
  // much.
  SiteEngagementService::Get(browser()->profile())
      ->ResetBaseScoreForURL(url, kHighEngagementScore);
  base::HistogramTester histograms;
  content::TestNavigationObserver observer(web_contents);
  const char* const kScript = "document.getElementById('form').submit()";
  EXPECT_TRUE(content::ExecuteScript(web_contents, kScript));
  observer.WaitForNavigationFinished();

  histograms.ExpectTotalCount(
      "Navigation.MainFrameFormSubmission.SiteEngagementLevel", 1);
  histograms.ExpectBucketCount(
      "Navigation.MainFrameFormSubmission.SiteEngagementLevel",
      blink::mojom::EngagementLevel::HIGH, 1);
}

}  // namespace
