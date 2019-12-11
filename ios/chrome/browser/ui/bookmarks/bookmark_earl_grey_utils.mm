// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_utils.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/tree_node_iterator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BookmarksMenuButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::CancelButton;
using chrome_test_util::ContextMenuCopyButton;
using chrome_test_util::OmniboxText;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

const GURL GetFirstUrl() {
  return web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
}

const GURL GetSecondUrl() {
  return web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
}

const GURL GetFrenchUrl() {
  return web::test::HttpServer::MakeUrl("http://www.a.fr/");
}

id<GREYMatcher> StarButton() {
  return ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);
}

id<GREYMatcher> BookmarksDeleteSwipeButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BOOKMARK_ACTION_DELETE);
}

id<GREYMatcher> NavigateBackButtonTo(NSString* previousViewControllerLabel) {
  // When using the stock UINavigationBar back button item, the button's label
  // may be truncated to the word "Back", or to nothing at all.  It is not
  // possible to know which label will be used, as the OS makes that decision,
  // so try to search for any of them.
  id<GREYMatcher> buttonLabelMatcher =
      grey_anyOf(grey_accessibilityLabel(previousViewControllerLabel),
                 grey_accessibilityLabel(@"Back"), nil);

  return grey_allOf(grey_kindOfClassName(@"UIButton"),
                    grey_ancestor(grey_kindOfClassName(@"UINavigationBar")),
                    buttonLabelMatcher, nil);
}

id<GREYMatcher> BookmarkHomeDoneButton() {
  return grey_accessibilityID(kBookmarkHomeNavigationBarDoneButtonIdentifier);
}

id<GREYMatcher> BookmarksSaveEditDoneButton() {
  return grey_accessibilityID(kBookmarkEditNavigationBarDoneButtonIdentifier);
}

id<GREYMatcher> BookmarksSaveEditFolderButton() {
  return grey_accessibilityID(
      kBookmarkFolderEditNavigationBarDoneButtonIdentifier);
}

id<GREYMatcher> ContextBarLeadingButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(kBookmarkHomeLeadingButtonIdentifier),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ContextBarCenterButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(kBookmarkHomeCenterButtonIdentifier),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ContextBarTrailingButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(kBookmarkHomeTrailingButtonIdentifier),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TappableBookmarkNodeWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(label), grey_sufficientlyVisible(),
                    nil);
}

id<GREYMatcher> SearchIconButton() {
  return grey_accessibilityID(kBookmarkHomeSearchBarIdentifier);
}

@implementation BookmarkEarlGreyUtils

+ (void)clearBookmarksPositionCache {
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browser_state->GetPrefs()];
}

+ (void)openBookmarks {
  // Opens the bookmark manager.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:BookmarksMenuButton()];

  // Assert the menu is gone.
  [[EarlGrey selectElementWithMatcher:BookmarksMenuButton()]
      assertWithMatcher:grey_nil()];
}

+ (void)openBookmarkFolder:(NSString*)bookmarkFolder {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"UITableViewCell"),
                                   grey_descendant(grey_text(bookmarkFolder)),
                                   nil)] performAction:grey_tap()];
}

+ (void)setupStandardBookmarks {
  [BookmarkEarlGreyUtils waitForBookmarkModelLoaded:YES];

  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  NSString* firstTitle = @"First URL";
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(firstTitle), GetFirstUrl());

  NSString* secondTitle = @"Second URL";
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(secondTitle), GetSecondUrl());

  NSString* frenchTitle = @"French URL";
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(frenchTitle), GetFrenchUrl());

  NSString* folderTitle = @"Folder 1";
  const bookmarks::BookmarkNode* folder1 = bookmark_model->AddFolder(
      bookmark_model->mobile_node(), 0, base::SysNSStringToUTF16(folderTitle));
  folderTitle = @"Folder 1.1";
  bookmark_model->AddFolder(bookmark_model->mobile_node(), 0,
                            base::SysNSStringToUTF16(folderTitle));

  folderTitle = @"Folder 2";
  const bookmarks::BookmarkNode* folder2 = bookmark_model->AddFolder(
      folder1, 0, base::SysNSStringToUTF16(folderTitle));

  folderTitle = @"Folder 3";
  const bookmarks::BookmarkNode* folder3 = bookmark_model->AddFolder(
      folder2, 0, base::SysNSStringToUTF16(folderTitle));

  const GURL thirdURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/chromium_logo_page.html");
  NSString* thirdTitle = @"Third URL";
  bookmark_model->AddURL(folder3, 0, base::SysNSStringToUTF16(thirdTitle),
                         thirdURL);
}

+ (void)setupBookmarksWhichExceedsScreenHeight {
  [BookmarkEarlGreyUtils waitForBookmarkModelLoaded:YES];

  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  const GURL dummyURL = web::test::HttpServer::MakeUrl("http://google.com");
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(@"Bottom URL"), dummyURL);

  NSString* dummyTitle = @"Dummy URL";
  for (int i = 0; i < 20; i++) {
    bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                           base::SysNSStringToUTF16(dummyTitle), dummyURL);
  }
  NSString* folderTitle = @"Folder 1";
  const bookmarks::BookmarkNode* folder1 = bookmark_model->AddFolder(
      bookmark_model->mobile_node(), 0, base::SysNSStringToUTF16(folderTitle));
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(@"Top URL"), dummyURL);

  // Add URLs to Folder 1.
  bookmark_model->AddURL(folder1, 0, base::SysNSStringToUTF16(dummyTitle),
                         dummyURL);
  bookmark_model->AddURL(folder1, 0, base::SysNSStringToUTF16(@"Bottom 1"),
                         dummyURL);
  for (int i = 0; i < 20; i++) {
    bookmark_model->AddURL(folder1, 0, base::SysNSStringToUTF16(dummyTitle),
                           dummyURL);
  }
}

+ (void)openMobileBookmarks {
  [BookmarkEarlGreyUtils openBookmarkFolder:@"Mobile Bookmarks"];
}

+ (void)assertBookmarksWithTitle:(NSString*)title
                   expectedCount:(NSUInteger)expectedCount {
  // Get BookmarkModel and wait for it to be loaded.
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  // Verify the correct number of bookmarks exist.
  base::string16 matchString = base::SysNSStringToUTF16(title);
  std::vector<bookmarks::TitledUrlMatch> matches;
  bookmarkModel->GetBookmarksMatching(matchString, 50, &matches);
  const size_t count = matches.size();
  GREYAssertEqual(expectedCount, count, @"Unexpected number of bookmarks");
}

+ (void)bookmarkCurrentTabWithTitle:(NSString*)title {
  [BookmarkEarlGreyUtils waitForBookmarkModelLoaded:YES];
  // Add the bookmark from the UI.
  [BookmarkEarlGreyUtils starCurrentTab];

  // Set the bookmark name.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_ACTION_EDIT)]
      performAction:grey_tap()];
  NSString* titleIdentifier = @"Title Field_textField";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(titleIdentifier)]
      performAction:grey_replaceText(title)];

  // Dismiss the window.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditDoneButton()]
      performAction:grey_tap()];
}

+ (void)starCurrentTab {
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuAddToBookmarks),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      performAction:grey_tap()];
}

+ (void)assertFolderName:(NSString*)folderName {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(folderName), nil)]
      assertWithMatcher:grey_notNil()];
}

+ (void)addFolderWithName:(NSString*)name {
  // Wait for folder picker to appear.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on "Create New Folder."
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  // Verify the folder creator is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Change the name of the folder.
  if (name.length > 0) {
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(@"Title_textField")]
        performAction:grey_replaceText(name)];
  }

  // Tap the Done button.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];
}

+ (void)assertFolderExists:(NSString*)title {
  base::string16 folderTitle16(base::SysNSStringToUTF16(title));
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmark_model->root_node());
  BOOL folderExists = NO;

  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->is_url())
      continue;
    // This is a folder.
    if (bookmark->GetTitle() == folderTitle16) {
      folderExists = YES;
      break;
    }
  }

  NSString* assertMessage =
      [NSString stringWithFormat:@"Folder %@ doesn't exist", title];
  GREYAssert(folderExists, assertMessage);
}

+ (void)verifyPromoAlreadySeen:(BOOL)seen {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefs = browserState->GetPrefs();
  if (prefs->GetBoolean(prefs::kIosBookmarkPromoAlreadySeen) == seen) {
    return;
  }
  NSString* errorDesc = (seen)
                            ? @"Expected promo already seen, but it wasn't."
                            : @"Expected promo not already seen, but it was.";
  GREYFail(errorDesc);
}

+ (void)setPromoAlreadySeen:(BOOL)seen {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefs = browserState->GetPrefs();
  prefs->SetBoolean(prefs::kIosBookmarkPromoAlreadySeen, seen);
}

+ (void)waitForDeletionOfBookmarkWithTitle:(NSString*)title {
  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(title)]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
             @"Waiting for bookmark to go away");
}

+ (void)waitForUndoToastToGoAway {
  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
             @"Waiting for undo toast to go away");
}

+ (void)waitForBookmarkModelLoaded:(BOOL)loaded {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout,
                 ^{
                   return bookmarkModel->loaded() == loaded;
                 }),
             @"Bookmark model was not loaded");
}

+ (void)assertExistenceOfBookmarkWithURL:(NSString*)URL name:(NSString*)name {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  const bookmarks::BookmarkNode* bookmark =
      bookmarkModel->GetMostRecentlyAddedUserNodeForURL(
          GURL(base::SysNSStringToUTF16(URL)));
  GREYAssert(bookmark->GetTitle() == base::SysNSStringToUTF16(name),
             @"Could not find bookmark named %@ for %@", name, URL);
}

+ (void)assertAbsenceOfBookmarkWithURL:(NSString*)URL {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  const bookmarks::BookmarkNode* bookmark =
      bookmarkModel->GetMostRecentlyAddedUserNodeForURL(
          GURL(base::SysNSStringToUTF16(URL)));
  GREYAssert(!bookmark, @"There is a bookmark for %@", URL);
}

+ (void)renameBookmarkFolderWithFolderTitle:(NSString*)folderTitle {
  NSString* titleIdentifier = @"Title_textField";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(titleIdentifier)]
      performAction:grey_replaceText(folderTitle)];
}

+ (void)closeEditBookmarkFolder {
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];
}

+ (void)closeContextBarEditMode {
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarCancelString])]
      performAction:grey_tap()];
}

+ (void)selectUrlsAndTapOnContextBarButtonWithLabelId:(int)buttonLabelId {
  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URLs.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on Open All.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(buttonLabelId)]
      performAction:grey_tap()];
}

+ (void)verifyOrderOfTabsWithCurrentTabIndex:(NSUInteger)tabIndex {
  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFrenchUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Switch to the next Tab and verify "Second URL" appears.
  // TODO(crbug.com/695749): see we if can add switchToNextTab to
  // chrome_test_util so that we don't need to pass tabIndex here.
  [ChromeEarlGrey selectTabAtIndex:tabIndex + 1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetSecondUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Switch to the next Tab and verify "First URL" appears.
  [ChromeEarlGrey selectTabAtIndex:tabIndex + 2];
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFirstUrl().GetContent())]
      assertWithMatcher:grey_notNil()];
}

+ (void)assertChildCount:(size_t)count ofFolderWithName:(NSString*)name {
  base::string16 name16(base::SysNSStringToUTF16(name));
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());

  const bookmarks::BookmarkNode* folder = NULL;
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->is_folder() && bookmark->GetTitle() == name16) {
      folder = bookmark;
      break;
    }
  }
  GREYAssert(folder, @"No folder named %@", name);
  GREYAssertEqual(folder->children().size(), count,
                  @"Unexpected number of children in folder '%@': %" PRIuS
                   " instead of %" PRIuS,
                  name, folder->children().size(), count);
}

+ (void)verifyContextMenuForSingleURL {
  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ContextMenuCopyButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

+ (void)verifyContextMenuForMultiAndMixedSelection {
  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

+ (void)verifyContextBarInDefaultStateWithSelectEnabled:(BOOL)selectEnabled
                                       newFolderEnabled:(BOOL)newFolderEnabled {
  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Verify context bar shows enabled "New Folder" and enabled "Select".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarNewFolderString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   newFolderEnabled
                                       ? grey_enabled()
                                       : grey_accessibilityTrait(
                                             UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarMoreString])]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarSelectString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   selectEnabled
                                       ? grey_enabled()
                                       : grey_accessibilityTrait(
                                             UIAccessibilityTraitNotEnabled),
                                   nil)];
}

+ (void)verifyContextBarInEditMode {
  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarMoreString])]
      assertWithMatcher:grey_notNil()];
}

+ (void)verifyFolderFlowIsClosed {
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
}

+ (void)verifyEmptyBackgroundAppears {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkEmptyStateExplanatoryLabelIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

+ (void)removeBookmarkWithTitle:(NSString*)title {
  base::string16 name16(base::SysNSStringToUTF16(title));
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->GetTitle() == name16) {
      bookmarkModel->Remove(bookmark);
      return;
    }
  }
  GREYFail(@"Could not remove bookmark with name %@", title);
}

+ (void)moveBookmarkWithTitle:(NSString*)bookmarkTitle
            toFolderWithTitle:(NSString*)newFolder {
  base::string16 name16(base::SysNSStringToUTF16(bookmarkTitle));
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());
  const bookmarks::BookmarkNode* bookmark = iterator.Next();
  while (iterator.has_next()) {
    if (bookmark->GetTitle() == name16) {
      break;
    }
    bookmark = iterator.Next();
  }

  base::string16 folderName16(base::SysNSStringToUTF16(newFolder));
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iteratorFolder(
      bookmarkModel->root_node());
  const bookmarks::BookmarkNode* folder = iteratorFolder.Next();
  while (iteratorFolder.has_next()) {
    if (folder->GetTitle() == folderName16) {
      break;
    }
    folder = iteratorFolder.Next();
  }
  std::set<const bookmarks::BookmarkNode*> toMove;
  toMove.insert(bookmark);
  bookmark_utils_ios::MoveBookmarks(toMove, bookmarkModel, folder);
}

+ (void)verifyBookmarkFolderIsSeen:(NSString*)bookmarkFolder {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"UITableViewCell"),
                                   grey_descendant(grey_text(bookmarkFolder)),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

+ (void)scrollToTop {
  // On iOS 13 the settings menu appears as a card that can be dismissed with a
  // downward swipe, for this reason we need to swipe up programatically to
  // avoid dismissin the VC.
  GREYPerformBlock scrollToTopBlock =
      ^BOOL(id element, __strong NSError** error) {
        UIScrollView* view = base::mac::ObjCCastStrict<UIScrollView>(element);
        view.contentOffset = CGPointZero;
        return YES;
      };

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeTableViewIdentifier)]
      performAction:[GREYActionBlock actionWithName:@"Scroll to top"
                                       performBlock:scrollToTopBlock]];
}

+ (void)scrollToBottom {
  // Provide a start points since it prevents some tests timing out under
  // certain configurations.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeTableViewIdentifier)]
      performAction:grey_scrollToContentEdgeWithStartPoint(
                        kGREYContentEdgeBottom, 0.5, 0.5)];
}

+ (void)verifyFolderCreatedWithTitle:(NSString*)folderTitle {
  // scroll to bottom to make sure new folder appears.
  [BookmarkEarlGreyUtils scrollToBottom];
  // verify the folder is created.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(folderTitle)]
      assertWithMatcher:grey_notNil()];
  // verify the editable textfield is gone.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_editing_text")]
      assertWithMatcher:grey_notVisible()];
}

+ (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
             setParentFolderTo:(NSString*)destinationFolder
                          from:(NSString*)sourceFolder {
  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarMoreString])]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(menuButtonId)]
      performAction:grey_tap()];

  // Verify that the edit page (editor) is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Verify current parent folder for is correct.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(sourceFolder), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on Folder to open folder picker.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Verify folder picker UI is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select the new destination folder. Use grey_ancestor since
  // BookmarksHomeTableView might be visible on the background on non-compact
  // widthts, and there might be a "destinationFolder" node there as well.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(TappableBookmarkNodeWithLabel(destinationFolder),
                            grey_ancestor(grey_accessibilityID(
                                kBookmarkFolderPickerViewContainerIdentifier)),
                            nil)] performAction:grey_tap()];

  // Verify folder picker is dismissed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Verify parent folder has been changed in edit page.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(destinationFolder),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss edit page (editor).
  id<GREYMatcher> dismissMatcher = BookmarksSaveEditDoneButton();
  // If a folder is being edited use the EditFolder button dismiss matcher
  // instead.
  if ([editorId isEqualToString:kBookmarkFolderEditViewContainerIdentifier])
    dismissMatcher = BookmarksSaveEditFolderButton();
  [[EarlGrey selectElementWithMatcher:dismissMatcher] performAction:grey_tap()];

  // Verify the Editor was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];

  // Wait for Undo toast to go away from screen.
  [BookmarkEarlGreyUtils waitForUndoToastToGoAway];
}

+ (void)tapOnLongPressContextMenuButton:(int)menuButtonId
                                 onItem:(id<GREYMatcher>)item
                             openEditor:(NSString*)editorId
                        modifyTextField:(NSString*)textFieldId
                                     to:(NSString*)newName
                            dismissWith:(NSString*)dismissButtonId {
  // Invoke Edit through item context menu.
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(menuButtonId)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Edit textfield.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(textFieldId)]
      performAction:grey_replaceText(newName)];

  // Dismiss editor.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(dismissButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];
}

+ (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
               modifyTextField:(NSString*)textFieldId
                            to:(NSString*)newName
                   dismissWith:(NSString*)dismissButtonId {
  // Invoke Edit through context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarMoreString])]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(menuButtonId)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Edit textfield.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(textFieldId)]
      performAction:grey_replaceText(newName)];

  // Dismiss editor.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(dismissButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];
}

+ (NSString*)contextBarNewFolderString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_NEW_FOLDER);
}

+ (NSString*)contextBarDeleteString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_DELETE);
}

+ (NSString*)contextBarCancelString {
  return l10n_util::GetNSString(IDS_CANCEL);
}

+ (NSString*)contextBarSelectString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_EDIT);
}

+ (NSString*)contextBarMoreString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_MORE);
}

+ (void)createNewBookmarkFolderWithFolderTitle:(NSString*)folderTitle
                                   pressReturn:(BOOL)pressReturn {
  // Click on "New Folder".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeLeadingButtonIdentifier)]
      performAction:grey_tap()];

  NSString* titleIdentifier = @"bookmark_editing_text";

  // Type the folder title.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(titleIdentifier),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_replaceText(folderTitle)];

  // Press the keyboard return key.
  if (pressReturn) {
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_accessibilityID(titleIdentifier),
                                     grey_sufficientlyVisible(), nil)]
        performAction:grey_typeText(@"\n")];

    // Wait until the editing textfield is gone.
    [BookmarkEarlGreyUtils waitForDeletionOfBookmarkWithTitle:titleIdentifier];
  }
}

@end
