// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MENU_HELPER_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MENU_HELPER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_provider.h"

class Browser;
class GURL;
@class RecentTabsTableViewController;
@protocol RecentTabsPresentationDelegate;

namespace synced_sessions {
class DistantSession;
}

// Commands used to create context menu actions.
@protocol RecentTabsContextMenuDelegate

// Triggers the URL sharing flow for the given |URL| and |title|, with the
// origin |view| representing the UI component for that URL.
- (void)shareURL:(const GURL&)URL title:(NSString*)title fromView:(UIView*)view;

// Hides Sessions corresponding to the given |sectionIdentifier|.
- (void)removeSessionAtSessionSectionIdentifier:(NSInteger)sectionIdentifier;

// Returns Sessions corresponding to the given |sectionIdentifier|.
- (synced_sessions::DistantSession const*)sessionForSectionIdentifier:
    (NSInteger)sectionIdentifier;

@end

//  RecentTabsContextMenuHelper controls the creation of context menus,
// based on the given |browser|, |RecentTabsPresentationDelegate| and
// |RecentTabsTableViewController|.
@interface RecentTabsContextMenuHelper : NSObject <RecentTabsMenuProvider>
- (instancetype)initWithBrowser:(Browser*)browser
    recentTabsPresentationDelegate:
        (id<RecentTabsPresentationDelegate>)recentTabsPresentationDelegate
     recentTabsContextMenuDelegate:
         (id<RecentTabsContextMenuDelegate>)recentTabsContextMenuDelegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MENU_HELPER_H_
