// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_H_

#import <UIKit/UIKit.h>

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"
#include "url/gurl.h"

#define BookmarkEarlGrey \
  [BookmarkEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// GURL for the testing bookmark "First URL".
const GURL GetFirstUrl();

// GURL for the testing bookmark "Second URL".
const GURL GetSecondUrl();

// GURL for the testing bookmark "French URL".
const GURL GetFrenchUrl();

// Test methods that perform actions on Bookmarks. These methods may read or
// alter Chrome's internal state programmatically or via the UI, but in both
// cases will properly synchronize the UI for Earl Grey tests.
@interface BookmarkEarlGreyImpl : BaseEGTestHelperImpl

#pragma mark - Setup and Teardown

// Clear Bookmarks top most row position cache.
- (void)clearBookmarksPositionCache;

// Loads a set of default bookmarks in the model for the tests to use.
// GREYAssert is induced if test bookmarks can not be loaded.
- (void)setupStandardBookmarks;

#pragma mark - Promo

// Checks that the promo has already been seen or not. GREYAssert is induced if
// the opposite is true.
- (void)verifyPromoAlreadySeen:(BOOL)seen;

// Checks that the promo has already been seen or not.
- (void)setPromoAlreadySeen:(BOOL)seen;

// Sets that the promo has already been seen |times| number of times.
- (void)setPromoAlreadySeenNumberOfTimes:(int)times;

// Returns the number of times a Promo has been seen.
- (int)numberOfTimesPromoAlreadySeen;

// Sets up a FakeIdentity and returns the email of this Identity.
- (NSString*)setupFakeIdentity;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_H_
