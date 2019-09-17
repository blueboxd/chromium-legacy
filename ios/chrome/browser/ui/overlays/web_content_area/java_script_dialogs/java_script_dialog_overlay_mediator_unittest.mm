// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_source.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/chrome/browser/ui/alert_view_controller/test/fake_alert_consumer.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_mediator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/test/java_script_dialog_overlay_mediator_test.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeJavaScriptDialogOverlayMediator
    : JavaScriptDialogOverlayMediator {
  web::TestWebState _webState;
  std::unique_ptr<JavaScriptDialogSource> _source;
  std::string _message;
}
// Initializer for a mediator that has a JavaScriptDialogSource with |sourceURL|
// and |isMainFrame|.  |message| is the dialog's message text.
- (instancetype)initWithRequest:(OverlayRequest*)request
                      sourceURL:(const GURL&)sourceURL
                    isMainFrame:(BOOL)isMainFrame
                        message:(const std::string&)message;
@end

@implementation FakeJavaScriptDialogOverlayMediator

- (instancetype)initWithRequest:(OverlayRequest*)request
                      sourceURL:(const GURL&)sourceURL
                    isMainFrame:(BOOL)isMainFrame
                        message:(const std::string&)message {
  if (self = [super initWithRequest:request]) {
    _source = std::make_unique<JavaScriptDialogSource>(&_webState, sourceURL,
                                                       isMainFrame);
    _message = message;
  }
  return self;
}

@end

@implementation FakeJavaScriptDialogOverlayMediator (Subclassing)

- (const JavaScriptDialogSource&)requestSource {
  return *_source.get();
}

- (const std::string&)requestMessage {
  return _message;
}

@end

// Test fixture for JavaScriptDialogOverlayMediator.
using JavaScriptDialogOverlayMediatorTest = JavaScriptDialogOverlayMediatorTest;

// Tests the JavaScript dialog setup from the main frame.
TEST_F(JavaScriptDialogOverlayMediatorTest, MainFrame) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  const GURL kUrl("https://chromium.org");
  const std::string kMessage("Message");
  SetMediator([[FakeJavaScriptDialogOverlayMediator alloc]
      initWithRequest:request.get()
            sourceURL:kUrl
          isMainFrame:YES
              message:kMessage]);

  // For the main frame, the URL is already displayed in the location bar, so
  // there is no need to add source information.  The message is used as the
  // consumer's title in this case.
  EXPECT_NSEQ(base::SysUTF8ToNSString(kMessage), consumer().title);
}

// Tests the JavaScript dialog setup from an iframe.
TEST_F(JavaScriptDialogOverlayMediatorTest, IFrameSetup) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  const GURL kUrl("https://chromium.org");
  const std::string kMessage("Message");
  SetMediator([[FakeJavaScriptDialogOverlayMediator alloc]
      initWithRequest:request.get()
            sourceURL:kUrl
          isMainFrame:NO
              message:kMessage]);

  NSString* expected_title = l10n_util::GetNSString(
      IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
  EXPECT_NSEQ(expected_title, consumer().title);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kMessage), consumer().message);
}
