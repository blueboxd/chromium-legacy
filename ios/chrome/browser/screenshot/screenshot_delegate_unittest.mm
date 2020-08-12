// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screenshot/screenshot_delegate.h"

#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface_provider.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ScreenshotDelegateTest : public PlatformTest {
 protected:
  ScreenshotDelegateTest() {}
  ~ScreenshotDelegateTest() override {}

  void SetUp() override {
    browser_interface_ = [[StubBrowserInterface alloc] init];
    browser_interface_provider_ = [[StubBrowserInterfaceProvider alloc] init];
    if (@available(iOS 13, *))
      screenshot_service_ = OCMClassMock([UIScreenshotService class]);
  }

  void createScreenshotDelegate() {
    screenshotDelegate_ = [[ScreenshotDelegate alloc]
        initWithBrowserInterfaceProvider:browser_interface_provider_];
  }

  web::WebTaskEnvironment task_environment_;
  StubBrowserInterface* browser_interface_;
  StubBrowserInterfaceProvider* browser_interface_provider_;
  ScreenshotDelegate* screenshotDelegate_;
  id screenshot_service_;
};

// Tests that ScreenshotDelegate can be init with browserInterfaceProvider can
// be set and that data can be generated from it.
TEST_F(ScreenshotDelegateTest, ScreenshotService) {
  // Expected: Empty NSData.
  if (@available(iOS 13, *)) {
    auto web_state = std::make_unique<web::TestWebState>();
    TestBrowser browser;

    // Insert the web_state into the Browser.
    int insertion_index = browser.GetWebStateList()->InsertWebState(
        WebStateList::kInvalidIndex, std::move(web_state),
        WebStateList::INSERT_NO_FLAGS, WebStateOpener());
    browser.GetWebStateList()->ActivateWebStateAt(insertion_index);

    // Add the Browser to StubBrowserInterface.
    browser_interface_.browser = &browser;

    // Add the StubBrowserInterface to StubBrowserInterfaceProvider.
    browser_interface_provider_.currentInterface = browser_interface_;

    createScreenshotDelegate();

    __block bool callback_ran = false;
    [screenshotDelegate_ screenshotService:screenshot_service_
        generatePDFRepresentationWithCompletion:^(NSData* PDFData,
                                                  NSInteger indexOfCurrentPage,
                                                  CGRect rectInCurrentPage) {
          EXPECT_TRUE(PDFData);
          callback_ran = true;
        }];

    EXPECT_TRUE(callback_ran);
  }
}

// Tests that when ScreenshotDelegate's browserInterfaceProvider has a nil
// Browser screenshotService will return nil.
TEST_F(ScreenshotDelegateTest, NilBrowser) {
  // Expected: nil NSData.
  if (@available(iOS 13, *)) {
    // Add the StubBrowserInterface with no set Browser to
    // StubBrowserInterfaceProvider.
    browser_interface_provider_.currentInterface = browser_interface_;

    createScreenshotDelegate();

    __block bool callback_ran = false;
    [screenshotDelegate_ screenshotService:screenshot_service_
        generatePDFRepresentationWithCompletion:^(NSData* PDFData,
                                                  NSInteger indexOfCurrentPage,
                                                  CGRect rectInCurrentPage) {
          EXPECT_FALSE(PDFData);
          callback_ran = true;
        }];

    EXPECT_TRUE(callback_ran);
  }
}

// Tests that when ScreenshotDelegate's browserInterfaceProvider has a nil
// WebSatate screenshotService will return nil.
TEST_F(ScreenshotDelegateTest, NilWebState) {
  // Expected: nil NSData.
  if (@available(iOS 13, *)) {
    TestBrowser browser;

    // Add the empty Browser to StubBrowserInterface.
    browser_interface_.browser = &browser;

    // Add the StubBrowserInterface to StubBrowserInterfaceProvider.
    browser_interface_provider_.currentInterface = browser_interface_;

    createScreenshotDelegate();

    __block bool callback_ran = false;
    [screenshotDelegate_ screenshotService:screenshot_service_
        generatePDFRepresentationWithCompletion:^(NSData* PDFData,
                                                  NSInteger indexOfCurrentPage,
                                                  CGRect rectInCurrentPage) {
          EXPECT_FALSE(PDFData);
          callback_ran = true;
        }];

    EXPECT_TRUE(callback_ran);
  }
}
