// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/browser_state.h"

#import <WebKit/WebKit.h>

#include "base/supports_user_data.h"
#include "ios/web/public/browsing_data/cookie_blocking_mode.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
class TestSupportsUserData : public base::SupportsUserData {
 public:
  TestSupportsUserData() {}
  ~TestSupportsUserData() override {}
};
}  // namespace

using BrowserStateTest = PlatformTest;

TEST_F(BrowserStateTest, FromSupportsUserData_NullPointer) {
  DCHECK_EQ(static_cast<web::BrowserState*>(nullptr),
            web::BrowserState::FromSupportsUserData(nullptr));
}

TEST_F(BrowserStateTest, FromSupportsUserData_NonBrowserState) {
  TestSupportsUserData supports_user_data;
  DCHECK_EQ(static_cast<web::BrowserState*>(nullptr),
            web::BrowserState::FromSupportsUserData(&supports_user_data));
}

TEST_F(BrowserStateTest, FromSupportsUserData) {
  web::TestBrowserState browser_state;
  DCHECK_EQ(&browser_state,
            web::BrowserState::FromSupportsUserData(&browser_state));
}

// Tests that changing the cookie blocking mode causes the injected Javascript
// to change.
TEST_F(BrowserStateTest, SetCookieBlockingMode) {
  web::TestBrowserState browser_state;
  browser_state.SetCookieBlockingMode(web::CookieBlockingMode::kAllow);

  web::WKWebViewConfigurationProvider& config_provider =
      web::WKWebViewConfigurationProvider::FromBrowserState(&browser_state);
  NSArray* wkscripts = config_provider.GetWebViewConfiguration()
                           .userContentController.userScripts;
  EXPECT_EQ(wkscripts.count, 4U);

  NSArray<WKUserScript*>* original_scripts =
      [[NSArray alloc] initWithArray:wkscripts copyItems:NO];
  // Make sure that the WKUserScripts are the same across multiple fetches if
  // no changes have occured.
  ASSERT_TRUE(
      [original_scripts isEqualToArray:config_provider.GetWebViewConfiguration()
                                           .userContentController.userScripts]);

  browser_state.SetCookieBlockingMode(web::CookieBlockingMode::kBlock);

  NSArray<WKUserScript*>* updated_scripts =
      [[NSArray alloc] initWithArray:wkscripts copyItems:NO];

  EXPECT_FALSE([original_scripts isEqualToArray:updated_scripts]);
}
