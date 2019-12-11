// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_utils.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::OmniboxText;

// Bookmark folders integration tests for Chrome.
@interface BookmarksFoldersTestCase : ChromeTestCase
@end

@implementation BookmarksFoldersTestCase

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  [ChromeEarlGrey clearBookmarks];
  [BookmarkEarlGreyUtils clearBookmarksPositionCache];
}

#pragma mark - BookmarksTestFolders Tests

// Tests moving bookmarks into a new folder created in the moving process.
- (void)testCreateNewFolderWhileMovingBookmarks {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URLs.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on Move.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move the bookmark into a new folder.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarkEarlGreyUtils
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder (Change Folder) is Bookmarks folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Choose new parent folder (Change Folder).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Verify folder picker UI is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify Folder 2 only has one item.
  [BookmarkEarlGreyUtils assertChildCount:1 ofFolderWithName:@"Folder 2"];

  // Select Folder 2 as new Change Folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];

  // Verify folder picker is dismissed and folder creator is now visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Verify picked parent folder (Change Folder) is Folder 2.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Folder 2"), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Verify new folder has been created under Folder 2.
  [BookmarkEarlGreyUtils assertChildCount:2 ofFolderWithName:@"Folder 2"];

  // Verify new folder has two bookmarks.
  [BookmarkEarlGreyUtils assertChildCount:2
                         ofFolderWithName:@"Title For New Folder"];
}

- (void)testCantDeleteFolderBeingEdited {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Create a new folder and type "New Folder 1" without pressing return.
  NSString* newFolderTitle = @"New Folder";
  [BookmarkEarlGreyUtils createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                    pressReturn:NO];

  // Swipe action to try to delete the newly created folder while its name its
  // being edited.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"New Folder")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify the delete confirmation button doesn't show up.
  [[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      assertWithMatcher:grey_nil()];
}

- (void)testNavigateAwayFromFolderBeingEdited {
  [BookmarkEarlGreyUtils setupBookmarksWhichExceedsScreenHeight];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Verify bottom URL is not visible before scrolling to bottom (make sure
  // setupBookmarksWhichExceedsScreenHeight works as expected).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Bottom URL")]
      assertWithMatcher:grey_notVisible()];

  // Verify the top URL is visible (isn't covered by the navigation bar).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Top URL")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Test new folder could be created.  This verifies bookmarks scrolled to
  // bottom successfully for folder name editng.
  NSString* newFolderTitle = @"New Folder";
  [BookmarkEarlGreyUtils createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                    pressReturn:NO];

  // Scroll to top to navigate away from the folder being created.
  [BookmarkEarlGreyUtils scrollToTop];

  // Scroll back to the Folder being created.
  [BookmarkEarlGreyUtils scrollToBottom];

  // Folder should still be in Edit mode, because of this match for Value.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityValue(@"New Folder")]
      assertWithMatcher:grey_notNil()];
}

- (void)testDeleteSingleFolderNode {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarkEarlGreyUtils waitForDeletionOfBookmarkWithTitle:@"Folder 1"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUtils verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                        newFolderEnabled:YES];
}

- (void)testSwipeDownToDismissFromEditFolder {
  if (!base::ios::IsRunningOnOrLater(13, 0, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 12 and lower.");
  }
  if (!IsCollectionsCardPresentationStyleEnabled()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on when feature flag is off.");
  }

  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Check that the TableView is presented.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Test when current navigating folder is deleted in background, empty
// background should be shown with context bar buttons disabled.
- (void)testWhenCurrentFolderDeletedInBackground {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Enter Folder 1
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];

  // Enter Folder 2
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Delete the Folder 1 and Folder 2 programmatically in background.
  [BookmarkEarlGreyUtils removeBookmarkWithTitle:@"Folder 2"];
  [BookmarkEarlGreyUtils removeBookmarkWithTitle:@"Folder 1"];

  // Verify edit mode is close automatically (context bar switched back to
  // default state) and both select and new folder button are disabled.
  [BookmarkEarlGreyUtils verifyContextBarInDefaultStateWithSelectEnabled:NO
                                                        newFolderEnabled:NO];

  // Verify the empty background appears.
  [BookmarkEarlGreyUtils verifyEmptyBackgroundAppears];

  // Come back to Folder 1 (which is also deleted).
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Folder 1")]
      performAction:grey_tap()];

  // Verify both select and new folder button are disabled.
  [BookmarkEarlGreyUtils verifyContextBarInDefaultStateWithSelectEnabled:NO
                                                        newFolderEnabled:NO];

  // Verify the empty background appears.
  [BookmarkEarlGreyUtils verifyEmptyBackgroundAppears];

  // Come back to Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Mobile Bookmarks")]
      performAction:grey_tap()];

  // Ensure Folder 1.1 is seen, that means it successfully comes back to Mobile
  // Bookmarks.
  [BookmarkEarlGreyUtils verifyBookmarkFolderIsSeen:@"Folder 1.1"];
}

- (void)testLongPressOnSingleFolder {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss the context menu. On non compact width tap the Bookmarks TableView
  // to dismiss, since there might not be a cancel button.
  if ([ChromeEarlGrey isCompactWidth]) {
    [[EarlGrey
        selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kBookmarkHomeTableViewIdentifier)]
        performAction:grey_tap()];
  }

  // Come back to the root.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Bookmarks")]
      performAction:grey_tap()];

  // Long press on Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Mobile Bookmarks")]
      performAction:grey_longPress()];

  // Verify it doesn't show the context menu. (long press is disabled on
  // permanent node.)
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_nil()];
}

// Verify Edit functionality for single folder selection.
- (void)testEditFunctionalityOnSingleFolder {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // 1. Edit the folder title at edit page.

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];
  NSString* existingFolderTitle = @"Folder 1";
  NSString* newFolderTitle = @"New Folder Title";
  [BookmarkEarlGreyUtils renameBookmarkFolderWithFolderTitle:newFolderTitle];

  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify that the change has been made.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(existingFolderTitle)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      assertWithMatcher:grey_notNil()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUtils verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                        newFolderEnabled:YES];

  // 2. Move a single folder at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(newFolderTitle)]
      performAction:grey_tap()];

  // Move the "New Folder Title" to "Folder 1.1".
  [BookmarkEarlGreyUtils
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER
                  openEditor:kBookmarkFolderEditViewContainerIdentifier
           setParentFolderTo:@"Folder 1.1"
                        from:@"Mobile Bookmarks"];

  // Verify edit mode remains.
  [BookmarkEarlGreyUtils verifyContextBarInEditMode];

  // Close edit mode.
  [BookmarkEarlGreyUtils closeContextBarEditMode];

  // Navigate to "Folder 1.1" and verify "New Folder Title" is under it.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // 3. Test the cancel button at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      performAction:grey_tap()];

  // Tap cancel after modifying the title.
  [BookmarkEarlGreyUtils
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER
                  openEditor:kBookmarkFolderEditViewContainerIdentifier
             modifyTextField:@"Title_textField"
                          to:@"Dummy"
                 dismissWith:@"Cancel"];

  // Verify that the bookmark was not updated.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify edit mode is stayed.
  [BookmarkEarlGreyUtils verifyContextBarInEditMode];

  // 4. Test the delete button at edit page.

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarMoreString])]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditorDeleteButtonIdentifier)]
      performAction:grey_tap()];

  // Wait for Undo toast to go away from screen.
  [BookmarkEarlGreyUtils waitForUndoToastToGoAway];

  // Verify that the folder is deleted.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      assertWithMatcher:grey_notVisible()];

  // 5. Verify that when adding a new folder, edit mode will not mistakenly come
  // back (crbug.com/781783).

  // Create a new folder.
  [BookmarkEarlGreyUtils createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                    pressReturn:YES];

  // Tap on the new folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      performAction:grey_tap()];

  // Verify we enter the new folder. (instead of selecting it in edit mode).
  [BookmarkEarlGreyUtils verifyEmptyBackgroundAppears];
}

// Verify Move functionality on single folder through long press.
- (void)testMoveFunctionalityOnSingleFolder {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move the bookmark folder - "Folder 1" into a new folder.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarkEarlGreyUtils
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder for "Title For New Folder" folder is "Mobile
  // Bookmarks" folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Choose new parent folder for "Title For New Folder" folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Verify folder picker UI is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify Folder 2 only has one item.
  [BookmarkEarlGreyUtils assertChildCount:1 ofFolderWithName:@"Folder 2"];

  // Select Folder 2 as new parent folder for "Title For New Folder".
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 2")]
      performAction:grey_tap()];

  // Verify folder picker is dismissed and folder creator is now visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Verify picked parent folder is Folder 2.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Folder 2"), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [BookmarkEarlGreyUtils verifyFolderFlowIsClosed];

  // Verify new folder "Title For New Folder" has been created under Folder 2.
  [BookmarkEarlGreyUtils assertChildCount:2 ofFolderWithName:@"Folder 2"];

  // Verify new folder "Title For New Folder" has one bookmark folder.
  [BookmarkEarlGreyUtils assertChildCount:1
                         ofFolderWithName:@"Title For New Folder"];

  // Drill down to where "Folder 1.1" has been moved and assert it's presence.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 2")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Title For New Folder")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify Move functionality on multiple folder selection.
- (void)testMoveFunctionalityOnMultipleFolder {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select multiple folders.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarMoreString])]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move into a new folder. By tapping on the New Folder Cell.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarkEarlGreyUtils
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder for "Title For New Folder" folder is "Mobile
  // Bookmarks" folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [BookmarkEarlGreyUtils verifyFolderFlowIsClosed];

  // Wait for Undo toast to go away from screen.
  [BookmarkEarlGreyUtils waitForUndoToastToGoAway];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUtils verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                        newFolderEnabled:YES];

  // Verify new folder "Title For New Folder" has two bookmark folder.
  [BookmarkEarlGreyUtils assertChildCount:2
                         ofFolderWithName:@"Title For New Folder"];

  // Drill down to where "Folder 1.1" and "Folder 1" have been moved and assert
  // it's presence.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Title For New Folder")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testContextBarForSingleFolderSelection {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select Folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Tap Edit Folder.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify it shows edit view controller.  Uses notNil() instead of
  // sufficientlyVisible() because the large title in the navigation bar causes
  // less than 75% of the table view to be visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];
}

- (void)testContextMenuForMultipleFolderSelection {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select Folders.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarMoreString])]
      performAction:grey_tap()];

  [BookmarkEarlGreyUtils verifyContextMenuForMultiAndMixedSelection];
}

// Tests that the default folder bookmarks are saved in is updated to the last
// used folder.
- (void)testStickyDefaultFolder {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      performAction:grey_tap()];

  // Tap the Folder button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Create a new folder.
  [BookmarkEarlGreyUtils addFolderWithName:@"Sticky Folder"];

  // Verify that the editor is present.  Uses notNil() instead of
  // sufficientlyVisible() because the large title in the navigation bar causes
  // less than 75% of the table view to be visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Tap the Done button.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Close bookmarks
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];

  // Second, bookmark a page.

  // Verify that the bookmark that is going to be added is not in the
  // BookmarkModel.
  const GURL bookmarkedURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/fullscreen.html");
  NSString* const bookmarkedURLString =
      base::SysUTF8ToNSString(bookmarkedURL.spec());
  [BookmarkEarlGreyUtils assertBookmarksWithTitle:bookmarkedURLString
                                    expectedCount:0];
  // Open the page.
  std::string expectedURLContent = bookmarkedURL.GetContent();
  [ChromeEarlGrey loadURL:bookmarkedURL];
  [[EarlGrey selectElementWithMatcher:OmniboxText(expectedURLContent)]
      assertWithMatcher:grey_notNil()];

  // Verify that the folder has only one element.
  [BookmarkEarlGreyUtils assertChildCount:1 ofFolderWithName:@"Sticky Folder"];

  // Bookmark the page.
  [BookmarkEarlGreyUtils starCurrentTab];

  // Verify the snackbar title.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"Bookmarked to Sticky Folder")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the newly-created bookmark is in the BookmarkModel.
  [BookmarkEarlGreyUtils assertBookmarksWithTitle:bookmarkedURLString
                                    expectedCount:1];

  // Verify that the folder has now two elements.
  [BookmarkEarlGreyUtils assertChildCount:2 ofFolderWithName:@"Sticky Folder"];
}

// Tests the new folder name is committed when name editing is interrupted by
// navigating away.
- (void)testNewFolderNameCommittedOnNavigatingAway {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Create a new folder and type "New Folder 1" without pressing return.
  NSString* newFolderTitle = @"New Folder 1";
  [BookmarkEarlGreyUtils createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                    pressReturn:NO];

  // Interrupt the folder name editing by tapping on back.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Bookmarks")]
      performAction:grey_tap()];

  // Come back to Mobile Bookmarks.
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Verify folder name "New Folder 1" was committed.
  [BookmarkEarlGreyUtils verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and type "New Folder 2" without pressing return.
  newFolderTitle = @"New Folder 2";
  [BookmarkEarlGreyUtils createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                    pressReturn:NO];

  // Interrupt the folder name editing by tapping on done.
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];
  // Reopen bookmarks.
  [BookmarkEarlGreyUtils openBookmarks];

  // Verify folder name "New Folder 2" was committed.
  [BookmarkEarlGreyUtils verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and type "New Folder 3" without pressing return.
  newFolderTitle = @"New Folder 3";
  [BookmarkEarlGreyUtils createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                    pressReturn:NO];

  // Interrupt the folder name editing by entering Folder 1
  [BookmarkEarlGreyUtils scrollToTop];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];
  // Come back to Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Mobile Bookmarks")]
      performAction:grey_tap()];

  // Verify folder name "New Folder 3" was committed.
  [BookmarkEarlGreyUtils verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and type "New Folder 4" without pressing return.
  newFolderTitle = @"New Folder 4";
  [BookmarkEarlGreyUtils createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                    pressReturn:NO];

  // Interrupt the folder name editing by tapping on First URL.
  [BookmarkEarlGreyUtils scrollToTop];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];
  // Reopen bookmarks.
  [BookmarkEarlGreyUtils openBookmarks];

  // Verify folder name "New Folder 4" was committed.
  [BookmarkEarlGreyUtils verifyFolderCreatedWithTitle:newFolderTitle];
}

// Tests the creation of new folders by tapping on 'New Folder' button of the
// context bar.
- (void)testCreateNewFolderWithContextBar {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Create a new folder and name it "New Folder 1".
  NSString* newFolderTitle = @"New Folder 1";
  [BookmarkEarlGreyUtils createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                    pressReturn:YES];

  // Verify "New Folder 1" is created.
  [BookmarkEarlGreyUtils verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and name it "New Folder 2".
  newFolderTitle = @"New Folder 2";
  [BookmarkEarlGreyUtils createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                    pressReturn:YES];

  // Verify "New Folder 2" is created.
  [BookmarkEarlGreyUtils verifyFolderCreatedWithTitle:newFolderTitle];

  // Verify context bar does not change after editing folder name.
  [BookmarkEarlGreyUtils verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                        newFolderEnabled:YES];
}

// Test the creation of a bookmark and new folder (by tapping on the star).
- (void)testAddBookmarkInNewFolder {
  const GURL bookmarkedURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  std::string expectedURLContent = bookmarkedURL.GetContent();

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [[EarlGrey selectElementWithMatcher:OmniboxText(expectedURLContent)]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGreyUtils starCurrentTab];

  // Verify the snackbar title.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Bookmarked")]
      assertWithMatcher:grey_notNil()];

  // Tap on the snackbar.
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  // Verify that the newly-created bookmark is in the BookmarkModel.
  [BookmarkEarlGreyUtils
      assertBookmarksWithTitle:base::SysUTF8ToNSString(expectedURLContent)
                 expectedCount:1];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGreyUtils assertFolderName:@"Mobile Bookmarks"];

  // Tap the Folder button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Create a new folder with default name.
  [BookmarkEarlGreyUtils addFolderWithName:nil];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGreyUtils assertFolderExists:@"New Folder"];
}

@end
