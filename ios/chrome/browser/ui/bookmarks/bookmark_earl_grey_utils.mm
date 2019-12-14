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
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
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
using chrome_test_util::BookmarksSaveEditDoneButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::CancelButton;
using chrome_test_util::ContextMenuCopyButton;
using chrome_test_util::OmniboxText;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

@implementation BookmarkEarlGreyUtils

#pragma mark - Public Interface

+ (void)clearBookmarksPositionCache {
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browser_state->GetPrefs()];
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
  [BookmarkEarlGreyUI starCurrentTab];

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

#pragma mark - Helpers

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

@end
