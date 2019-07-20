// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"

#include <memory>

#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/test/test_with_locale_and_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class CWVWebViewConfigurationTest : public TestWithLocaleAndResources {
 protected:
  web::TestWebThreadBundle web_thread_bundle_;
};

// Test CWVWebViewConfiguration initialization.
TEST_F(CWVWebViewConfigurationTest, Initialization) {
  std::unique_ptr<WebViewBrowserState> browser_state =
      std::make_unique<WebViewBrowserState>(/*off_the_record=*/false);
  WebViewBrowserState* browser_state_ptr = browser_state.get();
  CWVWebViewConfiguration* configuration = [[CWVWebViewConfiguration alloc]
      initWithBrowserState:std::move(browser_state)];
  EXPECT_EQ(browser_state_ptr, configuration.browserState);
  EXPECT_TRUE(configuration.persistent);
}

// Test CWVWebViewConfiguration properly shuts down.
TEST_F(CWVWebViewConfigurationTest, ShutDown) {
  std::unique_ptr<WebViewBrowserState> browser_state =
      std::make_unique<WebViewBrowserState>(/*off_the_record=*/false);
  CWVWebViewConfiguration* configuration = [[CWVWebViewConfiguration alloc]
      initWithBrowserState:std::move(browser_state)];
  EXPECT_TRUE(configuration.browserState);
  [configuration shutDown];
  EXPECT_FALSE(configuration.browserState);
}

}  // namespace ios_web_view
