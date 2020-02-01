// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/android/safe_browsing_api_handler.h"
#include "components/safe_browsing/content/base_blocking_page.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/load_completion_observer.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

void RunCallbackOnIOThread(
    std::unique_ptr<safe_browsing::SafeBrowsingApiHandler::URLCheckCallbackMeta>
        callback,
    safe_browsing::SBThreatType threat_type,
    const safe_browsing::ThreatMetadata& metadata) {
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(std::move(*callback), threat_type, metadata));
}

}  // namespace

class FakeSafeBrowsingApiHandler
    : public safe_browsing::SafeBrowsingApiHandler {
 public:
  // SafeBrowsingApiHandler
  std::string GetSafetyNetId() override { return ""; }
  void StartURLCheck(
      std::unique_ptr<URLCheckCallbackMeta> callback,
      const GURL& url,
      const safe_browsing::SBThreatTypeSet& threat_types) override {
    LOG(INFO) << "BLA test handler url=" + url.spec();
    RunCallbackOnIOThread(std::move(callback), GetSafeBrowsingRestriction(url),
                          safe_browsing::ThreatMetadata());
  }
  bool StartCSDAllowlistCheck(const GURL& url) override { return false; }
  bool StartHighConfidenceAllowlistCheck(const GURL& url) override {
    return false;
  }

  void AddRestriction(const GURL& url,
                      const safe_browsing::SBThreatType& threat_type) {
    restrictions_[url] = threat_type;
  }

 private:
  safe_browsing::SBThreatType GetSafeBrowsingRestriction(const GURL& url) {
    auto restrictions_iter = restrictions_.find(url);
    if (restrictions_iter == restrictions_.end()) {
      // if the url is not in restrictions assume it's safe.
      return safe_browsing::SB_THREAT_TYPE_SAFE;
    }
    return restrictions_iter->second;
  }

  std::map<GURL, safe_browsing::SBThreatType> restrictions_;
};

class SafeBrowsingBrowserTest : public WebLayerBrowserTest,
                                public ::testing::WithParamInterface<bool> {
 public:
  SafeBrowsingBrowserTest() : fake_handler_(new FakeSafeBrowsingApiHandler()) {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          safe_browsing::kCommittedSBInterstitials);
    } else {
      feature_list_.InitAndDisableFeature(
          safe_browsing::kCommittedSBInterstitials);
    }
  }
  ~SafeBrowsingBrowserTest() override = default;

  // WebLayerBrowserTest:
  void SetUpOnMainThread() override {
    NavigateAndWaitForCompletion(GURL("about:blank"), shell());
    safe_browsing::SafeBrowsingApiHandler::SetInstance(fake_handler_.get());
    ASSERT_TRUE(embedded_test_server()->Start());
    url_ = embedded_test_server()->GetURL("/simple_page.html");
  }

  void NavigateWithThreatType(const safe_browsing::SBThreatType& threatType,
                              bool expect_interstitial) {
    fake_handler_->AddRestriction(url_, threatType);
    Navigate(url_, expect_interstitial);
  }

  void Navigate(const GURL& url, bool expect_interstitial) {
    LoadCompletionObserver load_observer(shell());
    shell()->tab()->GetNavigationController()->Navigate(url);
    load_observer.Wait();
    EXPECT_EQ(expect_interstitial, HasInterstitial());
    if (expect_interstitial) {
      EXPECT_TRUE(GetSecurityInterstitialPage()->GetHTMLContents().length() >
                  0);
    }
  }

 protected:
  content::WebContents* GetWebContents() {
    Tab* tab = shell()->tab();
    TabImpl* tab_impl = static_cast<TabImpl*>(tab);
    return tab_impl->web_contents();
  }

  security_interstitials::SecurityInterstitialPage*
  GetSecurityInterstitialPage() {
    if (base::FeatureList::IsEnabled(
            safe_browsing::kCommittedSBInterstitials)) {
      security_interstitials::SecurityInterstitialTabHelper* helper =
          security_interstitials::SecurityInterstitialTabHelper::
              FromWebContents(GetWebContents());
      return helper
                 ? helper
                       ->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
                 : nullptr;
    }

    return static_cast<security_interstitials::SecurityInterstitialPage*>(
        content::InterstitialPage::GetInterstitialPage(GetWebContents())
            ->GetDelegateForTesting());
  }

  bool HasInterstitial() {
    if (base::FeatureList::IsEnabled(
            safe_browsing::kCommittedSBInterstitials)) {
      return GetSecurityInterstitialPage() != nullptr;
    }

    return GetWebContents()->GetInterstitialPage() != nullptr;
  }

  std::unique_ptr<FakeSafeBrowsingApiHandler> fake_handler_;
  GURL url_;
  base::test::ScopedFeatureList feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBrowserTest);
};

INSTANTIATE_TEST_SUITE_P(All, SafeBrowsingBrowserTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(SafeBrowsingBrowserTest,
                       DoesNotShowInterstitial_NoRestriction) {
  Navigate(url_, false);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBrowserTest, DoesNotShowInterstitial_Safe) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_SAFE, false);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBrowserTest, ShowsInterstitial_Malware) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_URL_MALWARE, true);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBrowserTest, ShowsInterstitial_Phishing) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_URL_PHISHING, true);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBrowserTest, ShowsInterstitial_Unwanted) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_URL_UNWANTED, true);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBrowserTest, ShowsInterstitial_Billing) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_BILLING, true);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBrowserTest,
                       ShowsInterstitial_Malware_Subresource) {
  GURL page_with_script_url =
      embedded_test_server()->GetURL("/simple_page_with_script.html");
  GURL script_url = embedded_test_server()->GetURL("/script.js");
  fake_handler_->AddRestriction(script_url,
                                safe_browsing::SB_THREAT_TYPE_URL_MALWARE);
  Navigate(page_with_script_url, true);
}

}  // namespace weblayer