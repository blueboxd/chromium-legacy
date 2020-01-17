// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_banner_interaction_handler.h"

#include "base/test/scoped_feature_list.h"
#include "components/infobars/core/infobar_feature.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/test/fake_infobar_ui_delegate.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for PasswordInfobarBannerInteractionHandler.
class PasswordInfobarBannerInteractionHandlerTest : public PlatformTest {
 public:
  PasswordInfobarBannerInteractionHandlerTest()
      : infobar_(
            [[FakeInfobarUIDelegate alloc] init],
            MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username",
                                                             @"password")) {
    scoped_feature_list_.InitWithFeatures({kIOSInfobarUIReboot}, {});
  }

  MockIOSChromeSavePasswordInfoBarDelegate& mock_delegate() {
    return *static_cast<MockIOSChromeSavePasswordInfoBarDelegate*>(
        infobar_.delegate());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  PasswordInfobarBannerInteractionHandler handler_;
  web::TestWebState web_state_;
  InfoBarIOS infobar_;
};

// Tests that BannerVisibilityChanged() calls InfobarPresenting() and
// InfobarDismissed() on the mock delegate.
TEST_F(PasswordInfobarBannerInteractionHandlerTest, Presentation) {
  EXPECT_CALL(mock_delegate(), InfobarPresenting(true));
  handler_.BannerVisibilityChanged(&infobar_, true);
  EXPECT_CALL(mock_delegate(), InfobarDismissed());
  handler_.BannerVisibilityChanged(&infobar_, false);
}

// Tests MainButtonTapped() calls Accept() on the mock delegate and resets
// the infobar to be accepted.
TEST_F(PasswordInfobarBannerInteractionHandlerTest, MainButton) {
  ASSERT_FALSE(infobar_.accepted());
  EXPECT_CALL(mock_delegate(), Accept()).WillOnce(testing::Return(true));
  handler_.MainButtonTapped(&infobar_);
  EXPECT_TRUE(infobar_.accepted());
}

// Tests that pressing the modal button calls adds an OverlayRequest for the
// modal UI to the WebState's queue at OverlayModality::kInfobarModal.
TEST_F(PasswordInfobarBannerInteractionHandlerTest, ShowModal) {
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kInfobarModal);
  ASSERT_EQ(0U, queue->size());
  handler_.ShowModalButtonTapped(&infobar_, &web_state_);
  // TODO(crbug.com/1033154): Verify that modal request has been added once
  // modal passwords UI is implemented.
}

// Tests that BannerVisibilityChanged() calls InfobarPresenting() and
// InfobarDismissed() on the mock delegate.
TEST_F(PasswordInfobarBannerInteractionHandlerTest, UserInitiatedDismissal) {
  EXPECT_CALL(mock_delegate(), InfoBarDismissed());
  handler_.BannerDismissedByUser(&infobar_);
}
