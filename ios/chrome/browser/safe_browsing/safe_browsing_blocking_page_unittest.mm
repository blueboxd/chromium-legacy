// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_blocking_page.h"

#include "base/strings/string_number_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_url_allow_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using safe_browsing::SBThreatType;
using security_interstitials::IOSSecurityInterstitialPage;
using security_interstitials::UnsafeResource;
using security_interstitials::SecurityInterstitialCommand;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kSpinDelaySeconds;

namespace {
// Creates an UnsafeResource for |web_state| using |url|.
UnsafeResource CreateResource(web::WebState* web_state, const GURL& url) {
  UnsafeResource resource;
  resource.url = url;
  resource.threat_type = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
  resource.web_state_getter = web_state->CreateDefaultGetter();
  return resource;
}
}  // namespace

// Test fixture for SafeBrowsingBlockingPage.
class SafeBrowsingBlockingPageTest : public PlatformTest {
 public:
  SafeBrowsingBlockingPageTest()
      : url_("http://www.chromium.test"),
        resource_(CreateResource(&web_state_, url_)),
        page_(SafeBrowsingBlockingPage::Create(resource_)) {
    std::unique_ptr<web::TestNavigationManager> navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
  }

  void SendCommand(SecurityInterstitialCommand command) {
    base::DictionaryValue dict;
    dict.SetKey("command", base::Value("." + base::NumberToString(command)));
    page_->HandleScriptCommand(dict, url_,
                               /*user_is_interacting=*/true,
                               /*sender_frame=*/nullptr);
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::IO_MAINLOOP};
  web::TestWebState web_state_;
  web::TestNavigationManager* navigation_manager_ = nullptr;
  GURL url_;
  UnsafeResource resource_;
  std::unique_ptr<IOSSecurityInterstitialPage> page_;
};

// Tests that the blocking page generates HTML.
TEST_F(SafeBrowsingBlockingPageTest, GenerateHTML) {
  EXPECT_GT(page_->GetHtmlContents().size(), 0U);
}

// Tests that the blocking page handles the proceed command by updating the
// allow list and reloading the page.
TEST_F(SafeBrowsingBlockingPageTest, HandleProceedCommand) {
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  ASSERT_FALSE(allow_list->AreUnsafeNavigationsAllowed(url_));
  ASSERT_FALSE(navigation_manager_->ReloadWasCalled());

  // Send the proceed command.
  SendCommand(security_interstitials::CMD_PROCEED);

  std::set<SBThreatType> allowed_threats;
  EXPECT_TRUE(allow_list->AreUnsafeNavigationsAllowed(url_, &allowed_threats));
  EXPECT_NE(allowed_threats.find(resource_.threat_type), allowed_threats.end());
  EXPECT_TRUE(navigation_manager_->ReloadWasCalled());
}

// Tests that the blocking page handles the don't proceed command by navigating
// back.
TEST_F(SafeBrowsingBlockingPageTest, HandleDontProceedCommand) {
  // Insert a safe navigation so that the page can navigate back to safety, then
  // add a navigation for the committed interstitial page.
  GURL kSafeUrl("http://www.safe.test");
  navigation_manager_->AddItem(kSafeUrl, ui::PAGE_TRANSITION_TYPED);
  navigation_manager_->AddItem(url_, ui::PAGE_TRANSITION_LINK);
  ASSERT_EQ(1, navigation_manager_->GetLastCommittedItemIndex());
  ASSERT_TRUE(navigation_manager_->CanGoBack());

  // Send the don't proceed command.
  SendCommand(security_interstitials::CMD_DONT_PROCEED);

  // Verify that the NavigationManager has navigated back.
  EXPECT_EQ(0, navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_FALSE(navigation_manager_->CanGoBack());
}

// Tests that the blocking page handles the don't proceed command by closing the
// WebState if there is no safe NavigationItem to navigate back to.
TEST_F(SafeBrowsingBlockingPageTest, HandleDontProceedCommandWithoutSafeItem) {
  // Send the don't proceed command.
  SendCommand(security_interstitials::CMD_DONT_PROCEED);

  // Wait for the WebState to be closed.  The close command run asynchronously
  // on the UI thread, so the runloop needs to be spun before it is handled.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kSpinDelaySeconds, ^{
    return web_state_.IsClosed();
  }));
}
