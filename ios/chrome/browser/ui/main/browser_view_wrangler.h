// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/app/application_mode.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"

@protocol ApplicationCommands;
@protocol BrowsingDataCommands;
class ChromeBrowserState;

namespace {

// Preference key used to store which profile is current.
NSString* kIncognitoCurrentKey = @"IncognitoActive";

}  // namespace

// Wrangler (a class in need of further refactoring) for handling the creation
// and ownership of BrowserViewController instances and their associated
// TabModels, and a few related methods.
@interface BrowserViewWrangler : NSObject <BrowserInterfaceProvider>

// Initialize a new instance of this class using |browserState| as the primary
// browser state for the tab models and BVCs, and setting
// |WebStateListObserving|, if not nil, as the webStateListObsever for any
// WebStateLists that are created. |applicationCommandEndpoint| is the object
// that methods in the ApplicationCommands protocol should be dispatched to by
// any BVCs that are created. |storageSwitcher| is used to manage changing any
// storage associated with the interfaces when the current interface changes;
// this is handled in the implementation of -setCurrentInterface:.
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
          applicationCommandEndpoint:
              (id<ApplicationCommands>)applicationCommandEndpoint
         browsingDataCommandEndpoint:
             (id<BrowsingDataCommands>)browsingDataCommandEndpoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, assign) NSString* sessionID;

// Creates the main Browser used by the receiver, using the browser state
// and tab model observer it was configured with. The main interface is then
// created; until this method is called, the main and incognito interfaces will
// be nil. This should be done before the main interface is accessed, usually
// immediately after initialization.
- (void)createMainBrowser;

// Destroy and rebuild the incognito Browser.
- (void)destroyAndRebuildIncognitoBrowser;

// Called before the instance is deallocated.
- (void)shutdown;

// Switch all global states for the given mode (normal or incognito).
- (void)switchGlobalStateToMode:(ApplicationMode)mode;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_
