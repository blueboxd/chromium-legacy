// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_unsafe_resource_container.h"

#include "base/bind.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using security_interstitials::UnsafeResource;

// Test fixture for SafeBrowsingUnsafeResourceContainer.
class SafeBrowsingUnsafeResourceContainerTest : public PlatformTest {
 public:
  SafeBrowsingUnsafeResourceContainerTest()
      : item_(web::NavigationItem::Create()) {
    std::unique_ptr<web::TestNavigationManager> navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    navigation_manager->SetLastCommittedItem(item_.get());
    web_state_.SetNavigationManager(std::move(navigation_manager));
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(&web_state_);
  }

  UnsafeResource MakeUnsafeResource(bool is_main_frame) {
    UnsafeResource resource;
    resource.url = GURL("http://www.chromium.test");
    resource.callback =
        base::BindRepeating([](bool proceed, bool showed_interstitial) {});
    resource.resource_type = is_main_frame
                                 ? safe_browsing::ResourceType::kMainFrame
                                 : safe_browsing::ResourceType::kSubFrame;
    resource.web_state_getter = web_state_.CreateDefaultGetter();
    return resource;
  }

  SafeBrowsingUnsafeResourceContainer* stack() {
    return SafeBrowsingUnsafeResourceContainer::FromWebState(&web_state_);
  }

 protected:
  std::unique_ptr<web::NavigationItem> item_;
  web::TestWebState web_state_;
};

// Tests that main frame resources are correctly stored in and released from the
// container.
TEST_F(SafeBrowsingUnsafeResourceContainerTest, MainFrameResource) {
  UnsafeResource resource = MakeUnsafeResource(/*is_main_frame=*/true);

  // The container should not have any unsafe main frame resources initially.
  EXPECT_FALSE(stack()->GetMainFrameUnsafeResource());

  // Store |resource| in the container.
  stack()->StoreUnsafeResource(resource);
  const UnsafeResource* resource_copy = stack()->GetMainFrameUnsafeResource();
  ASSERT_TRUE(resource_copy);
  EXPECT_EQ(resource.url, resource_copy->url);
  EXPECT_TRUE(resource_copy->callback.is_null());

  // Release the resource and check that it matches and that the callback has
  // been removed.
  std::unique_ptr<UnsafeResource> popped_resource =
      stack()->ReleaseMainFrameUnsafeResource();
  ASSERT_TRUE(popped_resource);
  EXPECT_EQ(resource.url, popped_resource->url);
  EXPECT_TRUE(popped_resource->callback.is_null());

  // Verify that the main frame resource was removed from the container.
  EXPECT_FALSE(stack()->GetMainFrameUnsafeResource());
  EXPECT_FALSE(stack()->ReleaseMainFrameUnsafeResource());
}

// Tests that subresources are correctly stored in and released from the
// container.
TEST_F(SafeBrowsingUnsafeResourceContainerTest, SubFrameResource) {
  UnsafeResource resource = MakeUnsafeResource(/*is_main_frame=*/false);

  // The container should not have any unsafe sub frame resources initially.
  EXPECT_FALSE(stack()->GetSubFrameUnsafeResource(item_.get()));

  // Store |resource| in the container.
  stack()->StoreUnsafeResource(resource);
  const UnsafeResource* resource_copy =
      stack()->GetSubFrameUnsafeResource(item_.get());
  ASSERT_TRUE(resource_copy);
  EXPECT_EQ(resource.url, resource_copy->url);
  EXPECT_TRUE(resource_copy->callback.is_null());

  // Release the resource and check that it matches and that the callback has
  // been removed.
  std::unique_ptr<UnsafeResource> popped_resource =
      stack()->ReleaseSubFrameUnsafeResource(item_.get());
  ASSERT_TRUE(popped_resource);
  EXPECT_EQ(resource.url, popped_resource->url);
  EXPECT_TRUE(popped_resource->callback.is_null());

  // Verify that the sub frame resource was removed from the container.
  EXPECT_FALSE(stack()->GetSubFrameUnsafeResource(item_.get()));
  EXPECT_FALSE(stack()->ReleaseSubFrameUnsafeResource(item_.get()));
}
