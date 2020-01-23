// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_SWITCHER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_SWITCHER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/application_commands.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class Browser;
@protocol TabSwitcher;
struct UrlLoadParams;

// This delegate is used to drive the TabSwitcher dismissal and execute code
// when the presentation and dismmiss animations finishes. The main controller
// is a good example of the implementation of this delegate.
@protocol TabSwitcherDelegate <NSObject>

// Informs the delegate the tab switcher should be dismissed with the given
// active browser.
- (void)tabSwitcher:(id<TabSwitcher>)tabSwitcher
    shouldFinishWithBrowser:(Browser*)browser
               focusOmnibox:(BOOL)focusOmnibox;

// Informs the delegate that the tab switcher is done and should be
// dismissed.
- (void)tabSwitcherDismissTransitionDidEnd:(id<TabSwitcher>)tabSwitcher;

@end

// This protocol describes the common interface between the two implementations
// of the tab switcher. StackViewController for iPhone and TabSwitcherController
// for iPad are examples of implementers of this protocol.
@protocol TabSwitcher <NSObject>

// This delegate must be set on the tab switcher in order to drive the tab
// switcher.
@property(nonatomic, weak) id<TabSwitcherDelegate> delegate;

// Restores the internal state of the tab switcher with the given browser,
// which must not be nil. |activeBrowser| is the browser which starts active,
// and must be one of the other two browsers. Should only be called when the
// object is not being shown.
- (void)restoreInternalStateWithMainBrowser:(Browser*)mainBrowser
                                 otrBrowser:(Browser*)otrBrowser
                              activeBrowser:(Browser*)activeBrowser;

// Returns the view controller that displays the tab switcher.
- (UIViewController*)viewController;

// Create a new tab in |browser|. Implementors are expected to also perform an
// animation from the selected tab in the tab switcher to the newly created tab
// in the content area. Objects adopting this protocol should call the following
// delegate methods:
//   |-tabSwitcher:shouldFinishWithBrowser:|
//   |-tabSwitcherDismissTransitionDidEnd:|
// to inform the delegate when this animation begins and ends.
- (void)dismissWithNewTabAnimationToBrowser:(Browser*)browser
                          withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
                                    atIndex:(int)position;

// Updates the OTR (Off The Record) browser. Should only be called when both
// the current OTR browser and the new OTR browser are either nil or contain no
// tabs. This must be called after the otr browser has been deleted because the
// incognito browser state is deleted.
- (void)setOtrBrowser:(Browser*)otrBrowser;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_SWITCHER_H_
