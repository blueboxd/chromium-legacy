// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/http_auth_dialogs/http_auth_dialog_overlay_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/alert_view_controller/test/fake_alert_consumer.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#import "ios/chrome/browser/ui/elements/text_field_configuration.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class HTTPAuthDialogOverlayMediatorTest : public PlatformTest {
 public:
  HTTPAuthDialogOverlayMediatorTest()
      : consumer_([[FakeAlertConsumer alloc] init]),
        message_("Message"),
        default_user_text_("Default Text"),
        request_(OverlayRequest::CreateWithConfig<HTTPAuthOverlayRequestConfig>(
            message_,
            default_user_text_)),
        mediator_([[HTTPAuthDialogOverlayMediator alloc]
            initWithRequest:request_.get()]) {
    mediator_.consumer = consumer_;
  }

 protected:
  FakeAlertConsumer* consumer_ = nil;
  const std::string message_;
  const std::string default_user_text_;
  std::unique_ptr<OverlayRequest> request_;
  HTTPAuthDialogOverlayMediator* mediator_ = nil;
};

// Tests that the consumer values are set correctly for confirmations.
TEST_F(HTTPAuthDialogOverlayMediatorTest, AlertSetup) {
  // Verify the consumer values.
  EXPECT_NSEQ(base::SysUTF8ToNSString(message_), consumer_.message);
  EXPECT_EQ(2U, consumer_.textFieldConfigurations.count);
  EXPECT_NSEQ(base::SysUTF8ToNSString(default_user_text_),
              consumer_.textFieldConfigurations[0].text);
  NSString* user_placeholer =
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_USERNAME_PLACEHOLDER);
  EXPECT_NSEQ(user_placeholer,
              consumer_.textFieldConfigurations[0].placeholder);
  EXPECT_FALSE(!!consumer_.textFieldConfigurations[1].text);
  NSString* password_placeholder =
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_PASSWORD_PLACEHOLDER);
  EXPECT_NSEQ(password_placeholder,
              consumer_.textFieldConfigurations[1].placeholder);
  ASSERT_EQ(2U, consumer_.actions.count);
  EXPECT_EQ(UIAlertActionStyleDefault, consumer_.actions[0].style);
  NSString* sign_in_label =
      l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL);
  EXPECT_NSEQ(sign_in_label, consumer_.actions[0].title);
  EXPECT_EQ(UIAlertActionStyleCancel, consumer_.actions[1].style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL), consumer_.actions[1].title);
}
