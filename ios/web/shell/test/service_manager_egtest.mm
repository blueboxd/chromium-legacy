// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/shell/test/earl_grey/service_manager_app_interface.h"
#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"
#import "ios/web/shell/test/earl_grey/web_shell_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ServiceManagerTestAppInterface)
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Asynchronously logs instance group name via "user_id" Mojo service. Logs are
// accessible via +logs method.
void MojoLogInstanceGroup() {
  [ServiceManagerTestAppInterface logInstanceGroup];
}

// Returns logs from |MojoLogInstanceGroup| and  methods.
NSArray* MojoGetLogs() {
  return [ServiceManagerTestAppInterface logs];
}

// Clears all Mojo logs.
void MojoClearLogs() {
  [ServiceManagerTestAppInterface clearLogs];
}

// Sets web usage flag for the current WebState using mojo API.
void MojoSetWebUsageEnabled(bool enabled) {
  [ServiceManagerTestAppInterface setWebUsageEnabled:enabled];
}

// Returns Mojo instance group name for current WebState.
NSString* GetInstanceGroup() {
  return [ShellEarlGrey instanceGroupForCurrentBrowserState];
}

}  // namespace

// Service Manager test cases for the web shell.
@interface ServiceManagerTestCase : WebShellTestCase
@end

@implementation ServiceManagerTestCase

- (void)setUp {
  [super setUp];
  MojoClearLogs();
}

- (void)tearDown {
  MojoClearLogs();
  [super tearDown];
}

// Tests that it is possible to connect to a per-user embedded service that
// was registered by web_shell.
- (void)testConnectionToPerUserEmbeddedService {
  MojoLogInstanceGroup();

  // Wait for log to appear.
  bool has_log = WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return MojoGetLogs().count == 1;
  });
  GREYAssert(has_log, @"No log appeared");
  GREYAssertEqualObjects(MojoGetLogs().firstObject, GetInstanceGroup(),
                         @"Different group names from Mojo and WebState");
}

// Tests that it is possible to connect to a per-WebState interface that is
// provided by web_shell.
- (void)testConnectionToWebStateInterface {
  // Ensure that web usage is enabled to start with.
  GREYAssert([ShellEarlGrey webUsageEnabledForCurrentWebState],
             @"WebState did not have expected initial state");

  MojoSetWebUsageEnabled(false);

  // Wait until the call altered |webState| as expected.
  bool disabled = WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return ![ShellEarlGrey webUsageEnabledForCurrentWebState];
  });
  GREYAssert(disabled, @"Failed to disable web usage");
}

@end
