// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screenshot/screenshot_notification_listener.h"

#import <UIKit/UIKit.h>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/test/metrics/user_action_tester.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
char const* singleScreenUserActionName = "MobileSingleScreenScreenshot";
}  // namespace

class ScreenshotNotificationListenerTest : public PlatformTest {
 protected:
  ScreenshotNotificationListenerTest() {}
  ~ScreenshotNotificationListenerTest() override {}

  void SetUp() override {
    sharedMock_ = OCMPartialMock([UIApplication sharedApplication]);
    screenshotNotificationListener_ =
        [[ScreenshotNotificationListener alloc] init];
    [screenshotNotificationListener_ startListening];
    if (@available(iOS 13, *))
      windowSceneMock_ = OCMClassMock([UIWindowScene class]);
  }

  void SendScreenshotNotification() {
    [NSNotificationCenter.defaultCenter
        postNotificationName:@"UIApplicationUserDidTakeScreenshotNotification"
                      object:nil
                    userInfo:nil];
  }

  id sharedMock_;
  id windowSceneMock_;
  base::UserActionTester user_action_tester_;
  ScreenshotNotificationListener* screenshotNotificationListener_;
};

// Tests when a UIApplicationUserDidTakeScreenshotNotification
// happens on a device with an iOS less than 13
TEST_F(ScreenshotNotificationListenerTest, iOS12Screenshot) {
  // Expected: Metric recorded
  if (@available(iOS 13, *))
    // If iOS13 test will automatically pass
    return

        SendScreenshotNotification();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(singleScreenUserActionName));
}

// Tests when a UIApplicationUserDidTakeScreenshotNotification
// happens on a device where multiple windows are not enabled.
TEST_F(ScreenshotNotificationListenerTest, iOS13MultiWindowNotEnabled) {
  // Expected: Metric recorded
  if (@available(iOS 13, *)) {
    OCMStub([sharedMock_ supportsMultipleScenes]).andReturn(NO);
    SendScreenshotNotification();
    EXPECT_EQ(1,
              user_action_tester_.GetActionCount(singleScreenUserActionName));
  }
}

// Tests that a metric is logged if there's a single screen with a single window
// and a UIApplicationUserDidTakeScreenshotNotification is sent.
TEST_F(ScreenshotNotificationListenerTest, iOS13SingleScreenSingleWindow) {
  // Expected: Metric recorded
  if (@available(iOS 13, *)) {
    OCMStub([sharedMock_ supportsMultipleScenes]).andReturn(YES);

    id windowSceneMock_ = OCMClassMock([UIWindowScene class]);
    // Mark the window as foregroundActive
    OCMStub([windowSceneMock_ activationState])
        .andReturn(UISceneActivationStateForegroundActive);

    NSSet* sceneSet = [NSSet setWithObject:windowSceneMock_];
    // Attatch it to the sharedApplication
    OCMStub([sharedMock_ connectedScenes]).andReturn(sceneSet);
    SendScreenshotNotification();
    EXPECT_EQ(1,
              user_action_tester_.GetActionCount(singleScreenUserActionName));
  }
}

// Tests that a metric is logged if there're are multiple screens each with
// a single window and a UIApplicationUserDidTakeScreenshotNotification is
// sent.
TEST_F(ScreenshotNotificationListenerTest, iOS13MultiScreenSingleWindow) {
  // Expected: Metric recorded
  if (@available(iOS 13, *)) {
    OCMStub([sharedMock_ supportsMultipleScenes]).andReturn(YES);

    // Mark the window as foregroundActive
    id foregroundWindowSceneMock = OCMClassMock([UIWindowScene class]);
    OCMStub([windowSceneMock_ activationState])
        .andReturn(UISceneActivationStateForegroundActive);

    // Mark the window as Background
    id backgroundWindowSceneMock = OCMClassMock([UIWindowScene class]);
    OCMStub([backgroundWindowSceneMock activationState])
        .andReturn(UISceneActivationStateBackground);

    NSSet* sceneSet =
        [[NSSet alloc] initWithObjects:foregroundWindowSceneMock,
                                       backgroundWindowSceneMock, nil];
    // Attatch the Scene State to the sharedApplication
    OCMStub([sharedMock_ connectedScenes]).andReturn(sceneSet);
    SendScreenshotNotification();
    EXPECT_EQ(1,
              user_action_tester_.GetActionCount(singleScreenUserActionName));
  }
}

// Tests that a metric is not logged if there is a multi-window screen in the
// foreground and a UIApplicationUserDidTakeScreenshotNotification is sent.
TEST_F(ScreenshotNotificationListenerTest, iOS13MultiScreenMultiWindow) {
  // Expected: Metric not recorded
  if (@available(iOS 13, *)) {
    OCMStub([sharedMock_ supportsMultipleScenes]).andReturn(YES);

    // Mark the window as foregroundActive
    id firstForegroundWindowSceneMock = OCMClassMock([UIWindowScene class]);
    OCMStub([windowSceneMock_ activationState])
        .andReturn(UISceneActivationStateForegroundActive);

    // Mark the window as foregroundActive
    id secondForegroundWindowSceneMock = OCMClassMock([UIWindowScene class]);
    OCMStub([secondForegroundWindowSceneMock activationState])
        .andReturn(UISceneActivationStateForegroundActive);

    NSSet* sceneSet =
        [[NSSet alloc] initWithObjects:firstForegroundWindowSceneMock,
                                       secondForegroundWindowSceneMock, nil];
    // Attatch the Scene State to the sharedApplication
    OCMStub([sharedMock_ connectedScenes]).andReturn(sceneSet);
    SendScreenshotNotification();
    EXPECT_EQ(0,
              user_action_tester_.GetActionCount(singleScreenUserActionName));
  }
}
