// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_helper.h"

#include "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#include "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class LookalikeUrlTabHelperTest : public PlatformTest {
 protected:
  LookalikeUrlTabHelperTest() {
    LookalikeUrlTabHelper::CreateForWebState(&web_state_);
    LookalikeUrlTabAllowList::CreateForWebState(&web_state_);
    LookalikeUrlContainer::CreateForWebState(&web_state_);
    allow_list_ = LookalikeUrlTabAllowList::FromWebState(&web_state_);
  }

  bool RequestAllowed(NSString* url_string, bool main_frame) {
    web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PageTransition::PAGE_TRANSITION_LINK, main_frame,
        /*has_user_gesture=*/false);
    web::WebStatePolicyDecider::PolicyDecision request_policy =
        web_state_.ShouldAllowRequest(
            [NSURLRequest requestWithURL:[NSURL URLWithString:url_string]],
            request_info);
    return request_policy.ShouldAllowNavigation();
  }

  LookalikeUrlTabAllowList* allow_list() { return allow_list_; }

 private:
  web::TestWebState web_state_;
  LookalikeUrlTabAllowList* allow_list_;
};

// Tests that ShouldAllowRequest properly blocks lookalike navigations and
// allows subframe navigations, non-HTTP/S navigations, and navigations
// to allowed domains.
TEST_F(LookalikeUrlTabHelperTest, ShouldAllowRequest) {
  NSString* lookalike_url = @"https://xn--googl-fsa.com/";

  // Lookalike IDNs should be blocked.
  EXPECT_FALSE(RequestAllowed(lookalike_url, /*main_frame=*/true));

  // Non-main frame navigations should be allowed.
  EXPECT_TRUE(RequestAllowed(lookalike_url, /*main_frame=*/false));
  // Non-HTTP/S navigations should be allowed.
  EXPECT_TRUE(
      RequestAllowed(@"file://xn--googl-fsa.com/", /*main_frame=*/true));

  // Lookalike IDNs that have been allowlisted should not be blocked.
  allow_list()->AllowDomain("xn--googl-fsa.com");
  EXPECT_TRUE(RequestAllowed(lookalike_url, /*main_frame=*/true));
}
