// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_UTILS_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_UTILS_H_

#import <Foundation/Foundation.h>

class GURL;
@protocol GREYMatcher;

// GURL for the testing bookmark "First URL".
const GURL GetFirstUrl();

// GURL for the testing bookmark "Second URL".
const GURL GetSecondUrl();

// GURL for the testing bookmark "French URL".
const GURL GetFrenchUrl();

// Matcher for bookmarks tool tip star. (used in iPad)
id<GREYMatcher> StarButton();

// Matcher for the Delete button on the bookmarks UI.
id<GREYMatcher> BookmarksDeleteSwipeButton();

// Matcher for the Back button to |previousViewControllerLabel| on the bookmarks
// UI.
id<GREYMatcher> NavigateBackButtonTo(NSString* previousViewControllerLabel);

// Matcher for the DONE button on the bookmarks UI.
id<GREYMatcher> BookmarkHomeDoneButton();

// Matcher for the DONE button on the bookmarks edit UI.
id<GREYMatcher> BookmarksSaveEditDoneButton();

// Matcher for the DONE button on the bookmarks edit folder UI.
id<GREYMatcher> BookmarksSaveEditFolderButton();

// Matcher for context bar leading button.
id<GREYMatcher> ContextBarLeadingButtonWithLabel(NSString* label);

// Matcher for context bar center button.
id<GREYMatcher> ContextBarCenterButtonWithLabel(NSString* label);

// Matcher for context bar trailing button.
id<GREYMatcher> ContextBarTrailingButtonWithLabel(NSString* label);

// Matcher for tappable bookmark node.
id<GREYMatcher> TappableBookmarkNodeWithLabel(NSString* label);

// Matcher for the search button.
id<GREYMatcher> SearchIconButton();

// Methods used for the EarlGrey tests.
@interface BookmarkEarlGreyUtils : NSObject

// Clear Bookmarks top most row position cache.
+ (void)clearBookmarksPositionCache;

// Navigates to the bookmark manager UI.
+ (void)openBookmarks;

// Selects |bookmarkFolder| to open.
+ (void)openBookmarkFolder:(NSString*)bookmarkFolder;

// Loads a set of default bookmarks in the model for the tests to use.
+ (void)setupStandardBookmarks;

// Loads a large set of bookmarks in the model which is longer than the screen
// height.
+ (void)setupBookmarksWhichExceedsScreenHeight;

// Selects MobileBookmarks to open.
+ (void)openMobileBookmarks;

// Asserts that |expectedCount| bookmarks exist with the corresponding |title|
// using the BookmarkModel.
+ (void)assertBookmarksWithTitle:(NSString*)title
                   expectedCount:(NSUInteger)expectedCount;

// Tap on the star to bookmark a page, then edit the bookmark to change the
// title to |title|.
+ (void)bookmarkCurrentTabWithTitle:(NSString*)title;

// Adds a bookmark for the current tab. Must be called when on a tab.
+ (void)starCurrentTab;

// Check that the currently edited bookmark is in |folderName| folder.
+ (void)assertFolderName:(NSString*)folderName;

// Creates a new folder starting from the folder picker.
// Passing a |name| of 0 length will use the default value.
+ (void)addFolderWithName:(NSString*)name;

// Asserts that a folder called |title| exists.
+ (void)assertFolderExists:(NSString*)title;

// Checks that the promo has already been seen or not.
+ (void)verifyPromoAlreadySeen:(BOOL)seen;

// Checks that the promo has already been seen or not.
+ (void)setPromoAlreadySeen:(BOOL)seen;

// Waits for the disparition of the given |title| in the UI.
+ (void)waitForDeletionOfBookmarkWithTitle:(NSString*)title;

// Wait for Undo toast to go away.
+ (void)waitForUndoToastToGoAway;

// Waits for the bookmark model to be loaded in memory.
+ (void)waitForBookmarkModelLoaded:(BOOL)loaded;

+ (void)assertExistenceOfBookmarkWithURL:(NSString*)URL name:(NSString*)name;

+ (void)assertAbsenceOfBookmarkWithURL:(NSString*)URL;

// Rename folder title to |folderTitle|. Must be in edit folder UI.
+ (void)renameBookmarkFolderWithFolderTitle:(NSString*)folderTitle;

// Dismisses the edit folder UI.
+ (void)closeEditBookmarkFolder;

// Close edit mode.
+ (void)closeContextBarEditMode;

// Select urls from Mobile Bookmarks and tap on a specified context bar button.
+ (void)selectUrlsAndTapOnContextBarButtonWithLabelId:(int)buttonLabelId;

// Verify the Mobile Bookmarks's urls are open in the same order as they are in
// folder.
+ (void)verifyOrderOfTabsWithCurrentTabIndex:(NSUInteger)tabIndex;

// Verifies that there is |count| children on the bookmark folder with |name|.
+ (void)assertChildCount:(size_t)count ofFolderWithName:(NSString*)name;

+ (void)verifyContextMenuForSingleURL;

+ (void)verifyContextMenuForMultiAndMixedSelection;

+ (void)verifyContextBarInDefaultStateWithSelectEnabled:(BOOL)selectEnabled
                                       newFolderEnabled:(BOOL)newFolderEnabled;

+ (void)verifyContextBarInEditMode;

+ (void)verifyFolderFlowIsClosed;

+ (void)verifyEmptyBackgroundAppears;

// Removes programmatically the first bookmark with the given title.
+ (void)removeBookmarkWithTitle:(NSString*)title;

+ (void)moveBookmarkWithTitle:(NSString*)bookmarkTitle
            toFolderWithTitle:(NSString*)newFolder;

+ (void)verifyBookmarkFolderIsSeen:(NSString*)bookmarkFolder;

// Scroll the bookmarks to top.
+ (void)scrollToTop;

// Scroll the bookmarks to bottom.
+ (void)scrollToBottom;

// Verify a folder with given name is created and it is not being edited.
+ (void)verifyFolderCreatedWithTitle:(NSString*)folderTitle;

+ (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
             setParentFolderTo:(NSString*)destinationFolder
                          from:(NSString*)sourceFolder;

+ (void)tapOnLongPressContextMenuButton:(int)menuButtonId
                                 onItem:(id<GREYMatcher>)item
                             openEditor:(NSString*)editorId
                        modifyTextField:(NSString*)textFieldId
                                     to:(NSString*)newName
                            dismissWith:(NSString*)dismissButtonId;

+ (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
               modifyTextField:(NSString*)textFieldId
                            to:(NSString*)newName
                   dismissWith:(NSString*)dismissButtonId;

// Context bar strings.
+ (NSString*)contextBarNewFolderString;

+ (NSString*)contextBarDeleteString;

+ (NSString*)contextBarCancelString;

+ (NSString*)contextBarSelectString;

+ (NSString*)contextBarMoreString;

// Create a new folder with given title.
+ (void)createNewBookmarkFolderWithFolderTitle:(NSString*)folderTitle
                                   pressReturn:(BOOL)pressReturn;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_UTILS_H_
