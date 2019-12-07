// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_overlay_tab_helper.h"

#import <Foundation/Foundation.h>

#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_banner_overlay_request_factory.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarDelegate;
using infobars::InfoBarManager;

namespace {
// Message for the fake InfoBar.
NSString* const kFakeInfoBarMessage = @"Fake Message";

// The pointer value stored in FakeOverlayRequestUserData used to configure
// banner OverlayRequests created for InfoBars using a fake InfoBarDelegate with
// kFakeInfoBarMessage.
void* kOverlayRequestConfigValue = &kOverlayRequestConfigValue;

// Test version of InfobarBannerOverlayRequestFactory.
class FakeInfobarBannerOverlayRequestFactory
    : public InfobarBannerOverlayRequestFactory {
 public:
  FakeInfobarBannerOverlayRequestFactory() = default;
  ~FakeInfobarBannerOverlayRequestFactory() override = default;

  // InfobarBannerOverlayRequestFactory:
  std::unique_ptr<OverlayRequest> CreateBannerRequest(
      InfoBar* infobar) override {
    return OverlayRequest::CreateWithConfig<FakeOverlayUserData>(
        kOverlayRequestConfigValue);
  }
};

}  // namespace

// Test fixture for InfobarOverlayTabHelper.
class InfobarOverlayTabHelperTest : public PlatformTest {
 public:
  InfobarOverlayTabHelperTest() {
    web_state_.SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    InfobarOverlayTabHelper::CreateForWebState(
        &web_state_,
        std::make_unique<FakeInfobarBannerOverlayRequestFactory>());
  }

  // Returns the front request of |web_state_|'s OverlayRequestQueue.
  OverlayRequest* front_request() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarBanner)
        ->front_request();
  }
  InfoBarManager* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }

 private:
  web::TestWebState web_state_;
};

// Tests that adding an InfoBar to the manager creates a fake banner request.
TEST_F(InfobarOverlayTabHelperTest, AddInfoBar) {
  ASSERT_FALSE(front_request());
  std::unique_ptr<InfoBarDelegate> delegate =
      std::make_unique<TestInfoBarDelegate>(kFakeInfoBarMessage);
  std::unique_ptr<InfoBar> infobar =
      std::make_unique<InfoBar>(std::move(delegate));
  manager()->AddInfoBar(std::move(infobar));
  ASSERT_TRUE(front_request());
}
