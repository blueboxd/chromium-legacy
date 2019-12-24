// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer_bridge.h"

#import "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture to test BreadcrumbManagerObserverBridge class.
class BreadcrumbManagerObserverBridgeTest : public PlatformTest {
 protected:
  BreadcrumbManagerObserverBridgeTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    breadcrumb_service_ =
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());

    mock_observer_ = OCMProtocolMock(@protocol(BreadcrumbManagerObserving));
    observer_bridge_ = std::make_unique<BreadcrumbManagerObserverBridge>(
        breadcrumb_service_, mock_observer_);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  BreadcrumbManagerKeyedService* breadcrumb_service_;
  id mock_observer_;
  std::unique_ptr<BreadcrumbManagerObserverBridge> observer_bridge_;
};

// Tests |breadcrumbManager:didAddEvent:| forwarding.
TEST_F(BreadcrumbManagerObserverBridgeTest, EventAdded) {
  std::string event("sample event");

  id event_parameter_validator = [OCMArg checkWithBlock:^BOOL(id value) {
    // The manager will prepended a timestamp to the event so verify that the
    // end matches |event|.
    return [value isKindOfClass:[NSString class]] &&
           [value hasSuffix:base::SysUTF8ToNSString(event)];
  }];
  OCMExpect([mock_observer_ breadcrumbManager:breadcrumb_service_
                                  didAddEvent:event_parameter_validator]);

  breadcrumb_service_->AddEvent(event);

  [mock_observer_ verify];
}
