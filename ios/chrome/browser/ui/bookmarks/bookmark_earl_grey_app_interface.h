// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// BookmarkEarlGreyAppInterface contains the app-side implementation for
// helpers that primarily work via direct model access. These helpers are
// compiled into the app binary and can be called from either app or test code.
@interface BookmarkEarlGreyAppInterface : NSObject

// Clear Bookmarks top most row position cache.
+ (void)clearBookmarksPositionCache;

// Loads a set of default bookmarks in the model for the tests to use.
+ (NSError*)setupStandardBookmarksUsingFirstURL:(NSString*)firstURL
                                      secondURL:(NSString*)secondURL
                                       thirdURL:(NSString*)thirdURL
                                      fourthURL:(NSString*)fourthURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_APP_INTERFACE_H_
