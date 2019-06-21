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
}
// Initializer for a mediator that has a JavaScriptDialogSource with |sourceURL|
// and |isMainFrame|.
- (instancetype)initWithRequest:(OverlayRequest*)request
                      sourceURL:(const GURL&)sourceURL
                    isMainFrame:(BOOL)isMainFrame;
@end

@implementation FakeJavaScriptDialogOverlayMediator

- (instancetype)initWithRequest:(OverlayRequest*)request
                      sourceURL:(const GURL&)sourceURL
                    isMainFrame:(BOOL)isMainFrame {
  if (self = [super initWithRequest:request]) {
    _source = std::make_unique<JavaScriptDialogSource>(&_webState, sourceURL,
                                                       isMainFrame);
  }
  return self;
}

@end

@implementation FakeJavaScriptDialogOverlayMediator (Subclassing)

- (const JavaScriptDialogSource*)requestSource {
  return _source.get();
}

@end

// Test fixture for JavaScriptDialogOverlayMediator.
using JavaScriptDialogOverlayMediatorTest = JavaScriptDialogOverlayMediatorTest;

// Tests the title for a JavaScript dialog from the main frame.
TEST_F(JavaScriptDialogOverlayMediatorTest, MainFrameTitle) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  const GURL kUrl("https://chromium.org");
  SetMediator([[FakeJavaScriptDialogOverlayMediator alloc]
      initWithRequest:request.get()
            sourceURL:kUrl
          isMainFrame:YES]);

  base::string16 title = url_formatter::FormatUrlForSecurityDisplay(
      kUrl, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  NSString* expected_title =
      l10n_util::GetNSStringF(IDS_JAVASCRIPT_MESSAGEBOX_TITLE, title);
  EXPECT_NSEQ(expected_title, consumer().title);
}

// Tests the title for a JavaScript dialog from the main frame.
TEST_F(JavaScriptDialogOverlayMediatorTest, IFrameTitle) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  const GURL kUrl("https://chromium.org");
  SetMediator([[FakeJavaScriptDialogOverlayMediator alloc]
      initWithRequest:request.get()
            sourceURL:kUrl
          isMainFrame:NO]);

  NSString* expected_title = l10n_util::GetNSString(
      IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
  EXPECT_NSEQ(expected_title, consumer().title);
}
