// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/core/features.h"
#import "ios/chrome/browser/safe_browsing/fake_safe_browsing_service.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
enum class SafeBrowsingDecisionTiming { kBeforeResponse, kAfterResponse };
}

class SafeBrowsingTabHelperTest
    : public testing::TestWithParam<SafeBrowsingDecisionTiming> {
 protected:
  SafeBrowsingTabHelperTest()
      : task_environment_(web::WebTaskEnvironment::IO_MAINLOOP) {
    feature_list_.InitAndEnableFeature(
        safe_browsing::kSafeBrowsingAvailableOnIOS);
    SafeBrowsingTabHelper::CreateForWebState(&web_state_);
  }

  // Whether Safe Browsing decisions arrive before calls to
  // ShouldAllowResponseUrl().
  bool SafeBrowsingDecisionArrivesBeforeResponse() const {
    return GetParam() == SafeBrowsingDecisionTiming::kBeforeResponse;
  }

  // Helper function that calls into WebState::ShouldAllowRequest with the
  // given |url| and |for_main_frame|.
  web::WebStatePolicyDecider::PolicyDecision ShouldAllowRequestUrl(
      const GURL& url,
      bool for_main_frame = true) {
    web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PageTransition::PAGE_TRANSITION_FIRST, for_main_frame,
        /*has_user_gesture=*/false);
    return web_state_.ShouldAllowRequest(
        [NSURLRequest requestWithURL:net::NSURLWithGURL(url)], request_info);
  }

  // Helper function that calls into WebState::ShouldAllowResponse with the
  // given |url| and |for_main_frame|, waits for the callback with the decision
  // to be called, and returns the decision.
  web::WebStatePolicyDecider::PolicyDecision ShouldAllowResponseUrl(
      const GURL& url,
      bool for_main_frame = true) {
    NSURLResponse* response =
        [[NSURLResponse alloc] initWithURL:net::NSURLWithGURL(url)
                                  MIMEType:@"text/html"
                     expectedContentLength:0
                          textEncodingName:nil];
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::Bind(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        });
    web_state_.ShouldAllowResponse(response, for_main_frame,
                                   std::move(callback));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called);
    return policy_decision;
  }

  web::WebTaskEnvironment task_environment_;
  web::TestWebState web_state_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests the case of a single navigation request and response, for a URL that is
// safe.
TEST_P(SafeBrowsingTabHelperTest, SingleSafeRequestAndResponse) {
  GURL url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a single navigation request and response, for a URL that is
// unsafe.
TEST_P(SafeBrowsingTabHelperTest, SingleUnsafeRequestAndResponse) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a single navigation request followed by multiple responses
// for the same URL.
TEST_P(SafeBrowsingTabHelperTest, RepeatedResponse) {
  GURL url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
  response_decision = ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
  response_decision = ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of multiple requests followed by multiple responses.
TEST_P(SafeBrowsingTabHelperTest, MultipleRequestsAndResponses) {
  GURL url1("http://chromium.test");
  GURL url2("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL url3("http://chromium3.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url1);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
  response_decision = ShouldAllowResponseUrl(url2);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
  response_decision = ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of multiple requests, followed by a single response skipping
// some of the request URLs.
TEST_P(SafeBrowsingTabHelperTest, MultipleRequestsSingleResponse) {
  GURL url1("http://chromium.test");
  GURL url2("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL url3("http://chromium3.test");
  GURL url4("http://chromium4.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(url4).ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of repeated requests for the same unsafe URL, ensuring that
// responses are not re-used.
TEST_P(SafeBrowsingTabHelperTest, RepeatedRequestsGetDistinctResponse) {
  // Compare the NSError objects.
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldDisplayError());

  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision2 =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision2.ShouldDisplayError());
  EXPECT_NE(response_decision.GetDisplayError(),
            response_decision2.GetDisplayError());
}

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    SafeBrowsingTabHelperTest,
    testing::Values(SafeBrowsingDecisionTiming::kBeforeResponse,
                    SafeBrowsingDecisionTiming::kAfterResponse));
