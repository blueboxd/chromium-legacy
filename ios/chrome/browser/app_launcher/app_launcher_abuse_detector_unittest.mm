// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/app_launcher/app_launcher_abuse_detector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// TODO(crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL SourceUrl1() {
  return GURL("http://www.google.com");
}
GURL SourceUrl2() {
  return GURL("http://www.goog.com");
}
GURL SourceUrl3() {
  return GURL("http://www.goog.ab");
}
GURL SourceUrl4() {
  return GURL("http://www.foo.com");
}
GURL AppUrl1() {
  return GURL("facetime://+1354");
}
GURL AppUrl2() {
  return GURL("facetime-audio://+1234");
}
GURL AppUrl3() {
  return GURL("abc://abc");
}
GURL AppUrl4() {
  return GURL("chrome://www.google.com");
}

}  // namespace

using AppLauncherAbuseDetectorTest = PlatformTest;

// Tests cases when the same app is launched repeatedly from same source.
TEST_F(AppLauncherAbuseDetectorTest,
       TestRepeatedAppLaunches_SameAppSameSource) {
  AppLauncherAbuseDetector* abuseDetector =
      [[AppLauncherAbuseDetector alloc] init];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:GURL("facetime://+154")
                            fromSourcePageURL:SourceUrl1()]);

  [abuseDetector didRequestLaunchExternalAppURL:GURL("facetime://+1354")
                              fromSourcePageURL:SourceUrl1()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:GURL("facetime://+12354")
                            fromSourcePageURL:SourceUrl1()]);

  [abuseDetector didRequestLaunchExternalAppURL:GURL("facetime://+154")
                              fromSourcePageURL:SourceUrl1()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:GURL("facetime://+13454")
                            fromSourcePageURL:SourceUrl1()]);

  [abuseDetector didRequestLaunchExternalAppURL:GURL("facetime://+14")
                              fromSourcePageURL:SourceUrl1()];
  // App was launched more than the max allowed times, the policy should change
  // to Prompt.
  EXPECT_EQ(ExternalAppLaunchPolicyPrompt,
            [abuseDetector launchPolicyForURL:GURL("facetime://+14")
                            fromSourcePageURL:SourceUrl1()]);
}

// Tests cases when same app is launched repeatedly from different sources.
TEST_F(AppLauncherAbuseDetectorTest,
       TestRepeatedAppLaunches_SameAppDifferentSources) {
  AppLauncherAbuseDetector* abuseDetector =
      [[AppLauncherAbuseDetector alloc] init];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl1()
                            fromSourcePageURL:SourceUrl1()]);
  [abuseDetector didRequestLaunchExternalAppURL:AppUrl1()
                              fromSourcePageURL:SourceUrl1()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl1()
                            fromSourcePageURL:SourceUrl1()]);

  [abuseDetector didRequestLaunchExternalAppURL:AppUrl1()
                              fromSourcePageURL:SourceUrl2()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl1()
                            fromSourcePageURL:SourceUrl2()]);
  [abuseDetector didRequestLaunchExternalAppURL:AppUrl1()
                              fromSourcePageURL:SourceUrl3()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl1()
                            fromSourcePageURL:SourceUrl3()]);
  [abuseDetector didRequestLaunchExternalAppURL:AppUrl1()
                              fromSourcePageURL:SourceUrl4()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl1()
                            fromSourcePageURL:SourceUrl4()]);
}

// Tests cases when different apps are launched from different sources.
TEST_F(AppLauncherAbuseDetectorTest,
       TestRepeatedAppLaunches_DifferentAppsDifferentSources) {
  AppLauncherAbuseDetector* abuseDetector =
      [[AppLauncherAbuseDetector alloc] init];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl1()
                            fromSourcePageURL:SourceUrl1()]);
  [abuseDetector didRequestLaunchExternalAppURL:AppUrl1()
                              fromSourcePageURL:SourceUrl1()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl1()
                            fromSourcePageURL:SourceUrl1()]);

  [abuseDetector didRequestLaunchExternalAppURL:AppUrl2()
                              fromSourcePageURL:SourceUrl2()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl2()
                            fromSourcePageURL:SourceUrl2()]);
  [abuseDetector didRequestLaunchExternalAppURL:AppUrl3()
                              fromSourcePageURL:SourceUrl3()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl3()
                            fromSourcePageURL:SourceUrl3()]);
  [abuseDetector didRequestLaunchExternalAppURL:AppUrl4()
                              fromSourcePageURL:SourceUrl4()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl4()
                            fromSourcePageURL:SourceUrl4()]);
}

// Tests blocking App launch only when the app have been allowed through the
// abuse detector before.
TEST_F(AppLauncherAbuseDetectorTest, TestBlockLaunchingApp) {
  AppLauncherAbuseDetector* abuseDetector =
      [[AppLauncherAbuseDetector alloc] init];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl1()
                            fromSourcePageURL:SourceUrl1()]);
  // Don't block for apps that have not been registered.
  [abuseDetector blockLaunchingAppURL:AppUrl1() fromSourcePageURL:SourceUrl1()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl1()
                            fromSourcePageURL:SourceUrl1()]);

  // Block for apps that have been registered
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl2()
                            fromSourcePageURL:SourceUrl2()]);
  [abuseDetector didRequestLaunchExternalAppURL:AppUrl2()
                              fromSourcePageURL:SourceUrl2()];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:AppUrl2()
                            fromSourcePageURL:SourceUrl2()]);
  [abuseDetector blockLaunchingAppURL:AppUrl2() fromSourcePageURL:SourceUrl2()];
  EXPECT_EQ(ExternalAppLaunchPolicyBlock,
            [abuseDetector launchPolicyForURL:AppUrl2()
                            fromSourcePageURL:SourceUrl2()]);
}
