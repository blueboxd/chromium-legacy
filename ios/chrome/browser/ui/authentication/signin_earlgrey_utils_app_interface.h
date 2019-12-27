// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARLGREY_UTILS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARLGREY_UTILS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@class FakeChromeIdentity;
@protocol GREYMatcher;

// SigninEarlGreyUtilsAppInterface contains the app-side implementation for
// helpers that primarily work via direct model access. These helpers are
// compiled into the app binary and can be called from either app or test code.
@interface SigninEarlGreyUtilsAppInterface : NSObject

// Returns a fake identity.
+ (FakeChromeIdentity*)fakeIdentity1;

// Returns a second fake identity.
+ (FakeChromeIdentity*)fakeIdentity2;

// Adds |fakeIdentity| to the fake identity service.
+ (void)addFakeIdentity:(FakeChromeIdentity*)fakeIdentity;

// Removes |fakeIdentity| from the fake chrome identity service, to simulate
// identity removal from the device.
+ (void)forgetFakeIdentity:(FakeChromeIdentity*)fakeIdentity;

// Returns the gaia ID of the signed-in account.
+ (NSString*)primaryAccountGaiaID;

// Checks that no identity is signed in.
+ (BOOL)isSignedOut;

// Returns a matcher for an identity picker cell for |email|.
+ (id<GREYMatcher>)identityCellMatcherForEmail:(NSString*)email;

// Removes |fakeIdentity| from the fake identity service.
+ (void)removeFakeIdentity:(FakeChromeIdentity*)fakeIdentity;

// Returns the current number of bookmarks from BookmarkModel.
+ (NSUInteger)bookmarkCount:(NSString*)title;

// Checks if any identity is currently authenticated.
+ (BOOL)isAuthenticated;

// Signs out the current user.
+ (void)signOut;

// Adds the new bookmark with URL and title to the |BookmarkModel|.
+ (void)addBookmark:(NSString*)urlString withTitle:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARLGREY_UTILS_APP_INTERFACE_H_
