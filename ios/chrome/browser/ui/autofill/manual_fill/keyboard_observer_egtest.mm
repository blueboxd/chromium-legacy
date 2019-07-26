// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <EarlGrey/GREYKeyboard.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const std::string kFormElementID1 = "username";
const std::string kFormElementID2 = "otherstuff";
const std::string kFormElementSubmit = "submit";

// If an element is focused in the webview, returns its ID. Returns an empty
// NSString otherwise.
NSString* GetFocusedElementID() {
  NSString* js =
      @"(function() {"
       "  return document.activeElement.id;"
       "})();";
  NSError* error = nil;
  NSString* result = chrome_test_util::ExecuteJavaScript(js, &error);
  GREYAssertNil(error, @"Unexpected error when executing JavaScript.");
  return result;
}

// Verifies that |elementId| is the selected element in the web page.
void AssertElementIsFocused(const std::string& element_id) {
  NSString* description =
      [NSString stringWithFormat:
                    @"Timeout waiting for the focused element in "
                    @"the webview to be \"%@\"",
                    base::SysUTF8ToNSString(element_id.c_str())];
  ConditionBlock condition = ^{
    return base::SysNSStringToUTF8(GetFocusedElementID()) == element_id;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
             description);
}

// Helper to tap a web element.
void TapOnWebElementWithID(const std::string& elementID) {
  [[EarlGrey
      selectElementWithMatcher:web::WebViewInWebState(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:web::WebViewTapElement(
                        chrome_test_util::GetCurrentWebState(),
                        [ElementSelector selectorWithElementID:elementID])];
}

}  // namespace

// Tests Mannual Fallback keyboard observer for form handling.
@interface KeyboardObserverTestCase : ChromeTestCase

// Observer to be tested.
@property(nonatomic, strong) KeyboardObserverHelper* keyboardObserver;

// Token to register a NSNotificationCenter observer.
@property(nonatomic, strong) id<NSObject> notificationToken;

// Delegate mock to confirm the observer callbacks.
@property(nonatomic, strong)
    OCMockObject<KeyboardObserverHelperConsumer>* keyboardObserverDelegateMock;

@end

@implementation KeyboardObserverTestCase

- (void)setUp {
  [super setUp];
  self.keyboardObserver = [[KeyboardObserverHelper alloc] init];
  self.keyboardObserverDelegateMock =
      OCMProtocolMock(@protocol(KeyboardObserverHelperConsumer));
  self.keyboardObserver.consumer = self.keyboardObserverDelegateMock;

  web::test::SetUpFileBasedHttpServer();
  GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/multi_field_form.html");
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];

  // Opening the keyboard from a webview blocks EarlGrey's synchronization.
  [[GREYConfiguration sharedInstance]
          setValue:@NO
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
}

- (void)tearDown {
  self.notificationToken = nil;
  self.keyboardObserverDelegateMock = nil;
  self.keyboardObserver = nil;

  // |setUp| disables synchronization.  Reenable here.
  [[GREYConfiguration sharedInstance]
          setValue:@YES
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
  [super tearDown];
}

// Tests the observer correctly identifies when the keyboard stays on screen.
- (void)testKeyboardDidStayOnScreen {
  // Brings up the keyboard by tapping on one of the form's field.
  TapOnWebElementWithID(kFormElementID1);

  // Wait for keyboard to finish animating.
  __block BOOL keyboardDidAppear = NO;
  self.notificationToken = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIKeyboardDidShowNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* note) {
                keyboardDidAppear = YES;
              }];
  ConditionBlock condition = ^{
    return keyboardDidAppear;
  };
  using base::test::ios::WaitUntilConditionOrTimeout;
  using base::test::ios::kWaitForUIElementTimeout;
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Wait for keyboard did show notification");

  // Verifies that the taped element is focused.
  AssertElementIsFocused(kFormElementID1);

  // Create a new callback expectation.
  OCMExpect([self.keyboardObserverDelegateMock keyboardDidStayOnScreen]);

  // Reset our keyboard boolean.
  keyboardDidAppear = NO;

  // Tap the second field.
  TapOnWebElementWithID(kFormElementID2);

  // Wait for keyboard to finish animating.
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Wait for keyboard did show notification");

  // Verifies that the taped element is focused.
  AssertElementIsFocused(kFormElementID2);

  // Verify the delegate call was made.
  [self.keyboardObserverDelegateMock verify];
}

// Tests that when the keyboard actually dismiss the right callback is done.
// TODO(crbug.com/914374): Address flakiness and reenable.
- (void)DISABLED_testKeyboardDidHide {
  // Brings up the keyboard by tapping on one of the form's field.
  TapOnWebElementWithID(kFormElementID1);

  // Verifies that the taped element is focused.
  AssertElementIsFocused(kFormElementID1);

  // Create the callback expectation.
  KeyboardState keyboardState = {NO, NO, NO, NO, NO};
  OCMExpect([self.keyboardObserverDelegateMock
      keyboardWillChangeToState:keyboardState]);

  // Tap the "Submit" button, and let the run loop spin.
  TapOnWebElementWithID(kFormElementSubmit);
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSeconds(1));

  // Verify the delegate call was made.
  [self.keyboardObserverDelegateMock verify];
}

@end
