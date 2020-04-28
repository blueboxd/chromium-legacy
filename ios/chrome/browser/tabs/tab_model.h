// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_MODEL_H_
#define IOS_CHROME_BROWSER_TABS_TAB_MODEL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

class ChromeBrowserState;
class SyncedWindowDelegateBrowserAgent;
class WebStateList;
class Browser;

// A model of a tab "strip". Although the UI representation may not be a
// traditional strip at all, tabs are still accessed via an integral index.
// The model knows about the currently selected tab in order to maintain
// consistency between multiple views that need the current tab to be
// synchronized.
@interface TabModel : NSObject

// The delegate for sync.
@property(nonatomic, readonly)
    SyncedWindowDelegateBrowserAgent* syncedWindowDelegate;

// BrowserState associated with this TabModel.
@property(nonatomic, readonly) ChromeBrowserState* browserState;

// YES if this tab set is off the record.
@property(nonatomic, readonly, getter=isOffTheRecord) BOOL offTheRecord;

// NO if the model has at least one tab.
@property(nonatomic, readonly, getter=isEmpty) BOOL empty;

// Determines the number of tabs in the model.
@property(nonatomic, readonly) NSUInteger count;

// The WebStateList owned by the TabModel.
@property(nonatomic, readonly) WebStateList* webStateList;

// Initializes tabs from existing browser object. |-setCurrentTab| needs to be
// called in order to display the views associated with the tabs. Waits until
// the views are ready.
- (instancetype)initWithBrowser:(Browser*)browser;

- (instancetype)init NS_UNAVAILABLE;

// Closes the tab at the given |index|. |index| must be valid.
- (void)closeTabAtIndex:(NSUInteger)index;

// Closes ALL the tabs.
- (void)closeAllTabs;

// Records tab session metrics.
- (void)recordSessionMetrics;

// Tells the receiver to disconnect from the model object it depends on. This
// should be called before destroying the browser state that the receiver was
// initialized with.
// It is safe to call this method multiple times.
// At this point the tab model will no longer ever be active, and will likely be
// deallocated soon. Calling any other methods or accessing any properties on
// the tab model after this is called is unsafe.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_H_
