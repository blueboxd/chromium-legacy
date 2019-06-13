// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/base_earl_grey_test_case.h"

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"
#import "ios/testing/earl_grey/coverage_utils.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(BaseEarlGreyTestCaseAppInterface)
#endif  // defined(CHROME_EARL_GREY_2)

@implementation BaseEarlGreyTestCase

+ (void)setUpForTestCase {
}

// Invoked upon starting each test method in a test case.
- (void)setUp {
  [super setUp];

#if defined(CHROME_EARL_GREY_2)
  [self launchAppForTestMethod];

  NSString* logFormat = @"*********************************\nStarting test: %@";
  [BaseEarlGreyTestCaseAppInterface
      logMessage:[NSString stringWithFormat:logFormat, self.name]];

  // Calling XCTFail before the application is launched does not assert
  // properly, so failing upon detection of overriding +setUp is delayed until
  // here. See +setUp below for details on why overriding +setUp causes a
  // failure.
  [self failIfSetUpIsOverridden];
#endif

  static dispatch_once_t setupToken;
  dispatch_once(&setupToken, ^{
    [CoverageUtils configureCoverageReportPath];
    [[self class] setUpForTestCase];
  });
}

- (void)launchAppForTestMethod {
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithArgs:nil
                                                 forceRestart:false];
}

// Prevents tests inheriting from this class from putting logic in +setUp.
// +setUp will be called before the application is launched,
// and thus is not suitable for most test case setup. Inheriting tests should
// migrate their +setUp logic to use the equivalent -setUpForTestCase.
- (void)failIfSetUpIsOverridden {
  if ([[BaseEarlGreyTestCase class] methodForSelector:@selector(setUp)] !=
      [[self class] methodForSelector:@selector(setUp)]) {
    XCTFail(@"EG2 test class %@ inheriting from BaseEarlGreyTestCase "
            @"should not override +setUp, as it is called before the "
            @"test application is launched. Please convert your "
            @"+setUp method to +setUpForTestCase.",
            NSStringFromClass([self class]));
  }
}

@end
