// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/url_checker_delegate_impl.h"

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/safe_browsing/core/db/database_manager.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_url_allow_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "net/http/http_request_headers.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForFileOperationTimeout;
using security_interstitials::UnsafeResource;

namespace {
// Struct used to test the execution of UnsafeResource callbacks.
struct UnsafeResourceCallbackState {
  bool executed = false;
  bool proceed = false;
  bool show_interstitial = false;
};
// Function used as the callback for UnsafeResources.
void PopulateCallbackState(UnsafeResourceCallbackState* state,
                           bool proceed,
                           bool show_interstitial) {
  state->executed = true;
  state->proceed = proceed;
  state->show_interstitial = show_interstitial;
}
}  // namespace

// Test fixture for UrlCheckerDelegateImpl.
class UrlCheckerDelegateImplTest : public PlatformTest {
 public:
  UrlCheckerDelegateImplTest()
      : delegate_(base::MakeRefCounted<UrlCheckerDelegateImpl>(nullptr)),
        item_(web::NavigationItem::Create()),
        web_state_(std::make_unique<web::TestWebState>()) {
    std::unique_ptr<web::TestNavigationManager> navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    navigation_manager->SetLastCommittedItem(item_.get());
    web_state_->SetNavigationManager(std::move(navigation_manager));
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(web_state_.get());
    SafeBrowsingUrlAllowList::CreateForWebState(web_state_.get());
  }
  ~UrlCheckerDelegateImplTest() override = default;

  // Creates an UnsafeResource whose callback populates |callback_state|.
  UnsafeResource CreateUnsafeResource(
      UnsafeResourceCallbackState* callback_state) {
    UnsafeResource resource;
    resource.url = GURL("http://www.chromium.test");
    resource.callback_thread = task_environment_.GetMainThreadTaskRunner();
    resource.callback =
        base::BindRepeating(&PopulateCallbackState, callback_state);
    resource.web_state_getter = web_state_->CreateDefaultGetter();
    return resource;
  }

  // Setter for whether the WebState is for prerendered content.
  void SetPrerendered(bool prerendered) {
    if (prerendered_ == prerendered)
      return;
    prerendered_ = prerendered;
    // The allow list and unsafe resource container are not created for
    // prerender WebState.
    if (prerendered_) {
      SafeBrowsingUrlAllowList::RemoveFromWebState(web_state_.get());
      SafeBrowsingUnsafeResourceContainer::RemoveFromWebState(web_state_.get());
    } else {
      SafeBrowsingUrlAllowList::CreateForWebState(web_state_.get());
      SafeBrowsingUnsafeResourceContainer::CreateForWebState(web_state_.get());
    }
  }

  // Waits for |state.executed| to be reset to true.  Returns whether the state
  // populated before a timeout.
  bool WaitForUnsafeResourceCallbackExecution(
      UnsafeResourceCallbackState* state) {
    task_environment_.RunUntilIdle();
    return WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
      return state->executed;
    });
  }

  // Getters for the allow list and unsafe resource container.
  SafeBrowsingUrlAllowList* allow_list() {
    return web_state_ ? SafeBrowsingUrlAllowList::FromWebState(web_state_.get())
                      : nullptr;
  }
  SafeBrowsingUnsafeResourceContainer* container() {
    return web_state_ ? SafeBrowsingUnsafeResourceContainer::FromWebState(
                            web_state_.get())
                      : nullptr;
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::IO_MAINLOOP};
  scoped_refptr<safe_browsing::UrlCheckerDelegate> delegate_;
  bool prerendered_ = false;
  std::unique_ptr<web::NavigationItem> item_;
  std::unique_ptr<web::TestWebState> web_state_;
};

// Tests that the delegate does not allow unsafe resources to proceed and does
// not show an interstitial for UnsafeResources for destroyed WebStates.
TEST_F(UrlCheckerDelegateImplTest, DontProceedForDestroyedWebState) {
  // Construct the UnsafeResource.
  UnsafeResourceCallbackState callback_state;
  UnsafeResource resource = CreateUnsafeResource(&callback_state);

  // Destroy the WebState.
  web_state_ = nullptr;

  // Instruct the delegate to display the blocking page.
  delegate_->StartDisplayingBlockingPageHelper(resource, /*method=*/"",
                                               net::HttpRequestHeaders(),
                                               /*is_main_frame*/ true,
                                               /*has_user_gesture=*/true);
  EXPECT_TRUE(WaitForUnsafeResourceCallbackExecution(&callback_state));

  // Verify the callback state.
  EXPECT_FALSE(callback_state.proceed);
  EXPECT_FALSE(callback_state.show_interstitial);
}

// Tests that the delegate does not allow unsafe resources to proceed and does
// not show an interstitial for UnsafeResources for prerender WebStates.
TEST_F(UrlCheckerDelegateImplTest, DontProceedForPrerenderWebState) {
  // Construct the UnsafeResource.
  UnsafeResourceCallbackState callback_state;
  UnsafeResource resource = CreateUnsafeResource(&callback_state);

  // Set the WebState to be prerendered.
  SetPrerendered(true);

  // Instruct the delegate to display the blocking page.
  delegate_->StartDisplayingBlockingPageHelper(resource, /*method=*/"",
                                               net::HttpRequestHeaders(),
                                               /*is_main_frame*/ true,
                                               /*has_user_gesture=*/true);
  EXPECT_TRUE(WaitForUnsafeResourceCallbackExecution(&callback_state));

  // Verify the callback state.
  EXPECT_FALSE(callback_state.proceed);
  EXPECT_FALSE(callback_state.show_interstitial);
}

// Tests that the delegate allows unsafe resources to proceed without showing an
// interstitial for allows unsafe navigations.
TEST_F(UrlCheckerDelegateImplTest, ProceedForAllowedUnsafeNavigation) {
  // Construct the UnsafeResource.
  safe_browsing::SBThreatType threat_type =
      safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  UnsafeResourceCallbackState callback_state;
  UnsafeResource resource = CreateUnsafeResource(&callback_state);
  resource.is_subresource = false;
  resource.is_subframe = false;
  resource.threat_type = threat_type;

  // Add the resource to the allow list.
  allow_list()->AllowUnsafeNavigations(resource.url, threat_type);

  // Instruct the delegate to display the blocking page.
  delegate_->StartDisplayingBlockingPageHelper(resource, /*method=*/"",
                                               net::HttpRequestHeaders(),
                                               /*is_main_frame*/ true,
                                               /*has_user_gesture=*/true);
  EXPECT_TRUE(WaitForUnsafeResourceCallbackExecution(&callback_state));

  // Verify the callback state.
  EXPECT_TRUE(callback_state.proceed);
  EXPECT_FALSE(callback_state.show_interstitial);
}

// Tests that the delegate registers a pending decision, stores the unsafe
// resource and does not allow unsafe resources to proceed for disallowed
// navigations on the main frame.
TEST_F(UrlCheckerDelegateImplTest,
       DontProceedAndSetUpInterstitialStateForMainFrame) {
  // Construct the UnsafeResource.
  safe_browsing::SBThreatType threat_type =
      safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  UnsafeResourceCallbackState callback_state;
  UnsafeResource resource = CreateUnsafeResource(&callback_state);
  resource.is_subresource = false;
  resource.is_subframe = false;
  resource.resource_type = safe_browsing::ResourceType::kMainFrame;
  resource.threat_type = threat_type;

  // Instruct the delegate to display the blocking page.
  delegate_->StartDisplayingBlockingPageHelper(resource, /*method=*/"",
                                               net::HttpRequestHeaders(),
                                               /*is_main_frame*/ true,
                                               /*has_user_gesture=*/true);
  EXPECT_TRUE(WaitForUnsafeResourceCallbackExecution(&callback_state));

  // Verify the callback state.
  EXPECT_FALSE(callback_state.proceed);
  EXPECT_TRUE(callback_state.show_interstitial);

  // Verify that the pending decision is recorded
  std::set<safe_browsing::SBThreatType> pending_threats;
  EXPECT_TRUE(allow_list()->IsUnsafeNavigationDecisionPending(
      resource.url, &pending_threats));
  EXPECT_EQ(1U, pending_threats.size());
  EXPECT_FALSE(pending_threats.find(threat_type) == pending_threats.end());

  // Verify that a copy of |resource| without its callback has been added to the
  // container.
  EXPECT_TRUE(container()->GetMainFrameUnsafeResource());
  std::unique_ptr<UnsafeResource> resource_copy =
      container()->ReleaseMainFrameUnsafeResource();
  ASSERT_TRUE(resource_copy);
  EXPECT_EQ(resource.url, resource_copy->url);
  EXPECT_EQ(resource.callback_thread, resource_copy->callback_thread);
  EXPECT_EQ(resource.web_state_getter, resource_copy->web_state_getter);
  EXPECT_EQ(resource.is_subresource, resource_copy->is_subresource);
  EXPECT_EQ(resource.is_subframe, resource_copy->is_subframe);
  EXPECT_EQ(resource.threat_type, resource_copy->threat_type);
}

// Tests that the delegate registers a pending decision, stores the unsafe
// resource and does not allow unsafe resources to proceed for disallowed
// navigations on a sub frame.
TEST_F(UrlCheckerDelegateImplTest,
       DontProceedAndSetUpInterstitialStateForSubFrame) {
  // Construct the UnsafeResource.
  safe_browsing::SBThreatType threat_type =
      safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  UnsafeResourceCallbackState callback_state;
  UnsafeResource resource = CreateUnsafeResource(&callback_state);
  resource.is_subresource = true;
  resource.is_subframe = true;
  resource.resource_type = safe_browsing::ResourceType::kSubFrame;
  resource.threat_type = threat_type;

  // Instruct the delegate to display the blocking page.
  delegate_->StartDisplayingBlockingPageHelper(resource, /*method=*/"",
                                               net::HttpRequestHeaders(),
                                               /*is_main_frame*/ false,
                                               /*has_user_gesture=*/true);
  EXPECT_TRUE(WaitForUnsafeResourceCallbackExecution(&callback_state));

  // Verify the callback state.
  EXPECT_FALSE(callback_state.proceed);
  EXPECT_TRUE(callback_state.show_interstitial);

  // Verify that the pending decision is recorded
  std::set<safe_browsing::SBThreatType> pending_threats;
  EXPECT_TRUE(allow_list()->IsUnsafeNavigationDecisionPending(
      resource.url, &pending_threats));
  EXPECT_EQ(1U, pending_threats.size());
  EXPECT_FALSE(pending_threats.find(threat_type) == pending_threats.end());

  // Verify that a copy of |resource| without its callback has been added to the
  // container.
  EXPECT_TRUE(container()->GetSubFrameUnsafeResource(item_.get()));
  std::unique_ptr<UnsafeResource> resource_copy =
      container()->ReleaseSubFrameUnsafeResource(item_.get());
  ASSERT_TRUE(resource_copy);
  EXPECT_EQ(resource.url, resource_copy->url);
  EXPECT_EQ(resource.callback_thread, resource_copy->callback_thread);
  EXPECT_EQ(resource.web_state_getter, resource_copy->web_state_getter);
  EXPECT_EQ(resource.is_subresource, resource_copy->is_subresource);
  EXPECT_EQ(resource.is_subframe, resource_copy->is_subframe);
  EXPECT_EQ(resource.threat_type, resource_copy->threat_type);
}
