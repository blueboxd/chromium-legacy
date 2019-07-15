// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils_app_interface.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(SigninEarlGreyUtilsAppInterface)
#endif  // defined(CHROME_EARL_GREY_2)

@implementation SigninEarlGreyUtilsImpl

- (ChromeIdentity*)fakeIdentity1 {
  return [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                        gaiaID:@"foo1ID"
                                          name:@"Fake Foo 1"];
}

- (ChromeIdentity*)fakeIdentity2 {
  return [FakeChromeIdentity identityWithEmail:@"foo2@gmail.com"
                                        gaiaID:@"foo2ID"
                                          name:@"Fake Foo 2"];
}

- (ChromeIdentity*)fakeManagedIdentity {
  return [FakeChromeIdentity identityWithEmail:@"foo@managed.com"
                                        gaiaID:@"fooManagedID"
                                          name:@"Fake Managed"
                                  hostedDomain:@"managed.com"];
}

- (void)checkSignedInWithIdentity:(ChromeIdentity*)identity {
  BOOL identityIsNonNil = identity != nil;
  EG_TEST_HELPER_ASSERT_TRUE(identityIsNonNil, @"Need to give an identity");

  // Required to avoid any problem since the following test is not dependant
  // to UI, and the previous action has to be totally finished before going
  // through the assert.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  NSString* primaryAccountGaiaID =
      [SignInEarlGreyUtilsAppInterface primaryAccountGaiaID];

  NSString* errorStr = [NSString
      stringWithFormat:@"Unexpected Gaia ID of the signed in user [expected = "
                       @"\"%@\", actual = \"%@\"]",
                       identity.gaiaID, primaryAccountGaiaID];
  EG_TEST_HELPER_ASSERT_TRUE(
      [identity.gaiaID isEqualToString:primaryAccountGaiaID], errorStr);
}

- (void)checkSignedOut {
  // Required to avoid any problem since the following test is not dependant to
  // UI, and the previous action has to be totally finished before going through
  // the assert.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  EG_TEST_HELPER_ASSERT_TRUE([SignInEarlGreyUtilsAppInterface isSignedOut],
                             @"Unexpected signed in user");
}

@end
