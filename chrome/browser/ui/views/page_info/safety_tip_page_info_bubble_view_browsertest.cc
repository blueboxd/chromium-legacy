// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/lookalikes/safety_tips/reputation_web_contents_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// An engagement score above MEDIUM.
const int kHighEngagement = 20;

// An engagement score below MEDIUM.
const int kLowEngagement = 1;

// Simulates a link click navigation. We don't use
// ui_test_utils::NavigateToURL(const GURL&) because it simulates the user
// typing the URL, causing the site to have a site engagement score of at
// least LOW.
void NavigateToURL(Browser* browser, const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
  params.initiator_origin = url::Origin::Create(GURL("about:blank"));
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.is_renderer_initiated = true;
  ui_test_utils::NavigateToURL(&params);
}

// Same as NavigateToUrl, but wait for the load to complete before returning.
void NavigateToURLSync(Browser* browser, const GURL& url) {
  content::TestNavigationObserver navigation_observer(
      browser->tab_strip_model()->GetActiveWebContents(), 1);
  NavigateToURL(browser, url);
  navigation_observer.Wait();
}

void PerformMouseClickOnView(views::View* view) {
  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  view->HandleAccessibleAction(data);
}

bool IsUIShowing() {
  return PageInfoBubbleViewBase::BUBBLE_SAFETY_TIP ==
         PageInfoBubbleViewBase::GetShownBubbleType();
}

void CloseWarningIgnore() {
  auto* widget = PageInfoBubbleViewBase::GetPageInfoBubble()->GetWidget();
  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  waiter.Wait();
}

// Sets the absolute Site Engagement |score| for the testing origin.
void SetEngagementScore(Browser* browser, const GURL& url, double score) {
  SiteEngagementService::Get(browser->profile())
      ->ResetBaseScoreForURL(url, score);
}

// Go to |navigated_url| in such a way as to trigger a warning. This is just for
// convenience, since how we trigger warnings will change.
void TriggerWarning(Browser* browser, const GURL& navigated_url) {
  SetEngagementScore(browser, navigated_url, kLowEngagement);
  NavigateToURLSync(browser, navigated_url);
}

}  // namespace

class SafetyTipPageInfoBubbleViewBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kSafetyTipUI);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetURL(const char* hostname) const {
    return embedded_test_server()->GetURL(hostname, "/title1.html");
  }

  GURL GetURLWithoutPath(const char* hostname) const {
    return GetURL(hostname).GetWithEmptyPath();
  }

  void ClickLeaveButton() {
    // This class is a friend to SafetyTipPageInfoBubbleView.
    auto* bubble = static_cast<SafetyTipPageInfoBubbleView*>(
        PageInfoBubbleViewBase::GetPageInfoBubble());
    PerformMouseClickOnView(bubble->GetLeaveButtonForTesting());
  }

  void CloseWarningLeaveSite(Browser* browser) {
    content::TestNavigationObserver navigation_observer(
        browser->tab_strip_model()->GetActiveWebContents(), 1);
    ClickLeaveButton();
    navigation_observer.Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// If the user has high engagement with a domain, it should not show a
// warning.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnHighEngagement) {
  auto kNavigatedUrl = GetURL("site1.com");
  SetEngagementScore(browser(), kNavigatedUrl, kHighEngagement);
  NavigateToURLSync(browser(), kNavigatedUrl);
  EXPECT_FALSE(IsUIShowing());
}

// Until we have heuristics, trigger on all low engagement sites.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       ShowOnLowEngagement) {
  auto kNavigatedUrl = GetURL("site1.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURLSync(browser(), kNavigatedUrl);
  EXPECT_TRUE(IsUIShowing());
}

// After the user clicks 'leave site', the user should end up on a safe domain.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       LeaveSiteLeavesSite) {
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarning(browser(), kNavigatedUrl);

  CloseWarningLeaveSite(browser());
  EXPECT_FALSE(IsUIShowing());
  EXPECT_NE(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// If the user clicks 'leave site', the warning should re-appear when the user
// re-visits the page.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       LeaveSiteStillWarnsAfter) {
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarning(browser(), kNavigatedUrl);

  CloseWarningLeaveSite(browser());

  NavigateToURLSync(browser(), kNavigatedUrl);

  EXPECT_TRUE(IsUIShowing());
  EXPECT_EQ(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// After the user closes the warning, they should still be on the same domain.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreWarningStaysOnPage) {
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarning(browser(), kNavigatedUrl);

  CloseWarningIgnore();
  EXPECT_FALSE(IsUIShowing());
  EXPECT_EQ(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// If the user closes the bubble, the warning should not re-appear when the user
// re-visits the page.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreWarningStopsWarning) {
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarning(browser(), kNavigatedUrl);

  CloseWarningIgnore();

  NavigateToURLSync(browser(), kNavigatedUrl);

  EXPECT_FALSE(IsUIShowing());
  EXPECT_EQ(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}
