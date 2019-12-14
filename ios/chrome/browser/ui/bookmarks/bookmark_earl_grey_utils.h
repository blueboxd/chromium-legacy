// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_UTILS_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_UTILS_H_

#import <Foundation/Foundation.h>

// Methods used for the EarlGrey tests.
@interface BookmarkEarlGreyUtils : NSObject

// Clear Bookmarks top most row position cache.
+ (void)clearBookmarksPositionCache;

// Loads a set of default bookmarks in the model for the tests to use.
+ (void)setupStandardBookmarks;

// Loads a large set of bookmarks in the model which is longer than the screen
// height.
+ (void)setupBookmarksWhichExceedsScreenHeight;

// Asserts that |expectedCount| bookmarks exist with the corresponding |title|
// using the BookmarkModel.
+ (void)assertBookmarksWithTitle:(NSString*)title
                   expectedCount:(NSUInteger)expectedCount;

// Tap on the star to bookmark a page, then edit the bookmark to change the
// title to |title|.
+ (void)bookmarkCurrentTabWithTitle:(NSString*)title;

// Asserts that a folder called |title| exists.
+ (void)assertFolderExists:(NSString*)title;

// Checks that the promo has already been seen or not.
+ (void)verifyPromoAlreadySeen:(BOOL)seen;

// Checks that the promo has already been seen or not.
+ (void)setPromoAlreadySeen:(BOOL)seen;

+ (void)assertExistenceOfBookmarkWithURL:(NSString*)URL name:(NSString*)name;

+ (void)assertAbsenceOfBookmarkWithURL:(NSString*)URL;

// Verify the Mobile Bookmarks's urls are open in the same order as they are in
// folder.
+ (void)verifyOrderOfTabsWithCurrentTabIndex:(NSUInteger)tabIndex;

// Verifies that there is |count| children on the bookmark folder with |name|.
+ (void)assertChildCount:(size_t)count ofFolderWithName:(NSString*)name;

// Removes programmatically the first bookmark with the given title.
+ (void)removeBookmarkWithTitle:(NSString*)title;

+ (void)moveBookmarkWithTitle:(NSString*)bookmarkTitle
            toFolderWithTitle:(NSString*)newFolder;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_UTILS_H_
