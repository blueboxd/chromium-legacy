// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/wpt/cwt_webdriver_app_interface.h"

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/testing/nserror_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

NSString* GetIdForTab(Tab* tab) {
  TabIdTabHelper::CreateForWebState(tab.webState);
  return TabIdTabHelper::FromWebState(tab.webState)->tab_id();
}

}  // namespace

@implementation CWTWebDriverAppInterface

+ (NSError*)loadURL:(NSString*)URL timeoutInSeconds:(NSTimeInterval)timeout {
  grey_dispatch_sync_on_main_thread(^{
    chrome_test_util::LoadUrl(GURL(base::SysNSStringToUTF8(URL)));
  });

  bool success = WaitUntilConditionOrTimeout(timeout, ^bool {
    __block BOOL isLoading = NO;
    grey_dispatch_sync_on_main_thread(^{
      isLoading = chrome_test_util::GetCurrentWebState()->IsLoading();
    });
    return !isLoading;
  });

  if (success)
    return nil;

  return testing::NSErrorWithLocalizedDescription(@"Page load timed out");
}

+ (NSString*)getCurrentTabID {
  __block NSString* tab_id = nil;
  grey_dispatch_sync_on_main_thread(^{
    Tab* currentTab = chrome_test_util::GetCurrentTab();
    if (currentTab)
      tab_id = GetIdForTab(currentTab);
  });

  return tab_id;
}

+ (NSArray*)getTabIDs {
  __block NSMutableArray* tabIDs;
  grey_dispatch_sync_on_main_thread(^{
    DCHECK(!chrome_test_util::IsIncognitoMode());
    NSUInteger tabCount = chrome_test_util::GetMainTabCount();
    tabIDs = [NSMutableArray arrayWithCapacity:tabCount];

    for (NSUInteger i = 0; i < tabCount; ++i) {
      Tab* tab = chrome_test_util::GetTabAtIndexInCurrentMode(i);
      [tabIDs addObject:GetIdForTab(tab)];
    }
  });

  return tabIDs;
}

+ (NSError*)closeCurrentTab {
  __block NSError* error = nil;
  grey_dispatch_sync_on_main_thread(^{
    if (chrome_test_util::GetCurrentTab())
      chrome_test_util::CloseCurrentTab();
    else
      error = testing::NSErrorWithLocalizedDescription(@"No current tab");
  });

  return error;
}

+ (NSError*)switchToTabWithID:(NSString*)ID {
  __block NSError* error = nil;
  grey_dispatch_sync_on_main_thread(^{
    DCHECK(!chrome_test_util::IsIncognitoMode());
    NSUInteger tabCount = chrome_test_util::GetMainTabCount();

    for (NSUInteger i = 0; i < tabCount; ++i) {
      Tab* tab = chrome_test_util::GetTabAtIndexInCurrentMode(i);
      if ([ID isEqualToString:GetIdForTab(tab)]) {
        chrome_test_util::SelectTabAtIndexInCurrentMode(i);
        return;
      }
    }
    error = testing::NSErrorWithLocalizedDescription(@"No matching tab");
  });

  return error;
}

@end
