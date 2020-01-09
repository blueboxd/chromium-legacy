// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_ADAPTOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_ADAPTOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_grid/tab_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/tab_switcher.h"

@class TabGridViewController;

// An opaque adaptor for the TabSwitcher protocol into the TabGrid.
// Consuming objects should be passed instances of this object as an
// id<TabSwitcher>.
// All of the methods and properties on this class are internal API fot the
// tab grid, and external code shouldn't depend on them.
@interface TabGridAdaptor : NSObject<TabSwitcher>
@property(nonatomic, weak) TabGridViewController* tabGridViewController;
// The mediator for the incognito grid.
@property(nonatomic, weak) TabGridMediator* incognitoMediator;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_ADAPTOR_H_
