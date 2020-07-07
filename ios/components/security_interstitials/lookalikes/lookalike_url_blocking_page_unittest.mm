// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_blocking_page.h"

#include "base/strings/string_number_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "ios/components/security_interstitials/lookalikes/lookalike_url_controller_client.h"
#include "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using security_interstitials::IOSSecurityInterstitialPage;
using security_interstitials::SecurityInterstitialCommand;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kSpinDelaySeconds;

namespace {
// Creates a LookalikeUrlBlockingPage with a given |safe_url|.
std::unique_ptr<LookalikeUrlBlockingPage> CreateBlockingPage(
    web::WebState* web_state,
    const GURL& safe_url,
    const GURL& request_url) {
  return std::make_unique<LookalikeUrlBlockingPage>(
      web_state, safe_url, request_url, ukm::kInvalidSourceId,
      LookalikeUrlMatchType::kSkeletonMatchTop500,
      std::make_unique<LookalikeUrlControllerClient>(web_state, safe_url,
                                                     request_url, "en-US"));
}
}  // namespace

// A Test web state that sets the visible URL to the last opened URL.
class TestWebState : public web::TestWebState {
 public:
  void OpenURL(const web::WebState::OpenURLParams& params) override {
    SetVisibleURL(params.url);
  }
};

// Test fixture for SafeBrowsingBlockingPage.
class LookalikeUrlBlockingPageTest : public PlatformTest {
 public:
  LookalikeUrlBlockingPageTest() : url_("https://www.chromium.test") {
    std::unique_ptr<web::TestNavigationManager> navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    LookalikeUrlTabAllowList::CreateForWebState(&web_state_);
    LookalikeUrlTabAllowList::FromWebState(&web_state_);
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
  TestWebState web_state_;
  web::TestNavigationManager* navigation_manager_ = nullptr;
  GURL url_;
  std::unique_ptr<IOSSecurityInterstitialPage> page_;
};

// Tests that the blocking page handles the proceed command by updating the
// allow list and reloading the page.
TEST_F(LookalikeUrlBlockingPageTest, HandleProceedCommand) {
  GURL safe_url("https://www.safe.test");
  page_ = CreateBlockingPage(&web_state_, safe_url, url_);
  LookalikeUrlTabAllowList* allow_list =
      LookalikeUrlTabAllowList::FromWebState(&web_state_);
  ASSERT_FALSE(allow_list->IsDomainAllowed(url_.host()));
  ASSERT_FALSE(navigation_manager_->ReloadWasCalled());

  // Send the proceed command.
  SendCommand(security_interstitials::CMD_PROCEED);

  EXPECT_TRUE(allow_list->IsDomainAllowed(url_.host()));
  EXPECT_TRUE(navigation_manager_->ReloadWasCalled());
}

// Tests that the blocking page handles the don't proceed command by navigating
// to the suggested URL.
TEST_F(LookalikeUrlBlockingPageTest, HandleDontProceedCommand) {
  GURL safe_url("https://www.safe.test");
  // Add a navigation for the committed interstitial page so that navigation to
  // the safe URL can later be verified.
  navigation_manager_->AddItem(url_, ui::PAGE_TRANSITION_LINK);
  page_ = CreateBlockingPage(&web_state_, safe_url, url_);

  // Send the don't proceed command.
  SendCommand(security_interstitials::CMD_DONT_PROCEED);

  EXPECT_EQ(web_state_.GetVisibleURL(), safe_url);
}

// Tests that the blocking page handles the don't proceed command by going back
// if there is no safe NavigationItem to navigate to.
TEST_F(LookalikeUrlBlockingPageTest,
       HandleDontProceedCommandWithoutSafeUrlGoBack) {
  // Insert a safe navigation so that the page can navigate back to safety, then
  // add a navigation for the committed interstitial page.
  GURL safe_url("https://www.safe.test");
  navigation_manager_->AddItem(safe_url, ui::PAGE_TRANSITION_TYPED);
  navigation_manager_->AddItem(url_, ui::PAGE_TRANSITION_LINK);
  ASSERT_EQ(1, navigation_manager_->GetLastCommittedItemIndex());
  ASSERT_TRUE(navigation_manager_->CanGoBack());

  page_ = CreateBlockingPage(&web_state_, GURL::EmptyGURL(), url_);

  // Send the don't proceed command.
  SendCommand(security_interstitials::CMD_DONT_PROCEED);

  // Verify that the NavigationManager has navigated back.
  EXPECT_EQ(0, navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_FALSE(navigation_manager_->CanGoBack());
}

// Tests that the blocking page handles the don't proceed command by closing the
// WebState if there is no safe NavigationItem to navigate to and unable to go
// back.
TEST_F(LookalikeUrlBlockingPageTest,
       HandleDontProceedCommandWithoutSafeUrlClose) {
  page_ = CreateBlockingPage(&web_state_, GURL::EmptyGURL(), url_);
  ASSERT_FALSE(navigation_manager_->CanGoBack());

  // Send the don't proceed command.
  SendCommand(security_interstitials::CMD_DONT_PROCEED);

  // Wait for the WebState to be closed.  The close command run asynchronously
  // on the UI thread, so the runloop needs to be spun before it is handled.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kSpinDelaySeconds, ^{
    return web_state_.IsClosed();
  }));
}
