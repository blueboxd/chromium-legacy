// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_app_interface.h"

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BookmarkEarlGreyAppInterface

#pragma mark - Public Interface

+ (void)clearBookmarksPositionCache {
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browser_state->GetPrefs()];
}

+ (NSError*)setupStandardBookmarksUsingFirstURL:(NSString*)firstURL
                                      secondURL:(NSString*)secondURL
                                       thirdURL:(NSString*)thirdURL
                                      fourthURL:(NSString*)fourthURL {
  if (![BookmarkEarlGreyAppInterface waitForBookmarkModelLoaded:YES])
    return testing::NSErrorWithLocalizedDescription(
        @"Bookmark model was not loaded");

  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  NSString* firstTitle = @"First URL";
  const GURL firstGURL = GURL(base::SysNSStringToUTF8(firstURL));
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(firstTitle), firstGURL);

  NSString* secondTitle = @"Second URL";
  const GURL secondGURL = GURL(base::SysNSStringToUTF8(secondURL));
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(secondTitle), secondGURL);

  NSString* frenchTitle = @"French URL";
  const GURL thirdGURL = GURL(base::SysNSStringToUTF8(thirdURL));
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(frenchTitle), thirdGURL);

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

  NSString* thirdTitle = @"Third URL";
  const GURL fourthGURL = GURL(base::SysNSStringToUTF8(fourthURL));
  bookmark_model->AddURL(folder3, 0, base::SysNSStringToUTF16(thirdTitle),
                         fourthGURL);
  return nil;
}

+ (NSError*)verifyPromoAlreadySeen:(BOOL)seen {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefs = browserState->GetPrefs();
  if (prefs->GetBoolean(prefs::kIosBookmarkPromoAlreadySeen) == seen) {
    return nil;
  }
  NSString* errorDescription =
      (seen) ? @"Expected promo already seen, but it wasn't."
             : @"Expected promo not already seen, but it was.";
  return testing::NSErrorWithLocalizedDescription(errorDescription);
}

+ (void)setPromoAlreadySeen:(BOOL)seen {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->SetBoolean(prefs::kIosBookmarkPromoAlreadySeen, seen);
}

+ (void)setPromoAlreadySeenNumberOfTimes:(int)times {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->SetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount, times);
}

+ (int)numberOfTimesPromoAlreadySeen {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  return prefs->GetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount);
}

+ (NSString*)setupFakeIdentity {
  FakeChromeIdentity* identity =
      [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                     gaiaID:@"foo1ID"
                                       name:@"Fake Foo 1"];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  return identity.userEmail;
}

#pragma mark - Helpers

+ (BOOL)waitForBookmarkModelLoaded:(BOOL)loaded {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return bookmarkModel->loaded() == loaded;
      });
}

@end
