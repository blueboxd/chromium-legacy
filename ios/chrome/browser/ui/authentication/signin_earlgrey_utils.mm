// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils_app_interface.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SigninEarlGreyUtilsImpl

- (FakeChromeIdentity*)fakeIdentity1 {
  return [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                        gaiaID:@"foo1ID"
                                          name:@"Fake Foo 1"];
}

- (FakeChromeIdentity*)fakeIdentity2 {
  return [FakeChromeIdentity identityWithEmail:@"foo2@gmail.com"
                                        gaiaID:@"foo2ID"
                                          name:@"Fake Foo 2"];
}

- (FakeChromeIdentity*)fakeManagedIdentity {
  return [FakeChromeIdentity identityWithEmail:@"foo@managed.com"
                                        gaiaID:@"fooManagedID"
                                          name:@"Fake Managed"];
}

- (void)addFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  [SigninEarlGreyUtilsAppInterface addFakeIdentity:fakeIdentity];
}

- (void)forgetFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  [SigninEarlGreyUtilsAppInterface forgetFakeIdentity:fakeIdentity];
}

- (void)checkSignedInWithFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  BOOL fakeIdentityIsNonNil = fakeIdentity != nil;
  EG_TEST_HELPER_ASSERT_TRUE(fakeIdentityIsNonNil, @"Need to give an identity");

  // Required to avoid any problem since the following test is not dependant
  // to UI, and the previous action has to be totally finished before going
  // through the assert.
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^bool {
                   NSString* primaryAccountGaiaID =
                       [SigninEarlGreyUtilsAppInterface primaryAccountGaiaID];
                   return primaryAccountGaiaID.length > 0;
                 }),
             @"Sign in did not complete.");
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  NSString* primaryAccountGaiaID =
      [SigninEarlGreyUtilsAppInterface primaryAccountGaiaID];

  NSString* errorStr = [NSString
      stringWithFormat:@"Unexpected Gaia ID of the signed in user [expected = "
                       @"\"%@\", actual = \"%@\"]",
                       fakeIdentity.gaiaID, primaryAccountGaiaID];
  EG_TEST_HELPER_ASSERT_TRUE(
      [fakeIdentity.gaiaID isEqualToString:primaryAccountGaiaID], errorStr);
}

- (void)checkSignedOut {
  // Required to avoid any problem since the following test is not dependant to
  // UI, and the previous action has to be totally finished before going through
  // the assert.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  EG_TEST_HELPER_ASSERT_TRUE([SigninEarlGreyUtilsAppInterface isSignedOut],
                             @"Unexpected signed in user");
}

- (void)removeFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  [SigninEarlGreyUtilsAppInterface removeFakeIdentity:fakeIdentity];
}

@end
