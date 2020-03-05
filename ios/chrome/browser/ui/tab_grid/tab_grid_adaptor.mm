// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_adaptor.h"

#include "base/logging.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridAdaptor
// TabSwitcher properties.
@synthesize delegate = _delegate;

#pragma mark - TabSwitcher

- (void)restoreInternalStateWithMainBrowser:(Browser*)mainBrowser
                                 otrBrowser:(Browser*)otrBrowser
                              activeBrowser:(Browser*)activeBrowser {
  // The only action here is to signal to the tab grid which panel should be
  // active.
  if (activeBrowser == otrBrowser) {
    self.tabGridViewController.activePage = TabGridPageIncognitoTabs;
  } else {
    self.tabGridViewController.activePage = TabGridPageRegularTabs;
  }
}

- (UIViewController*)viewController {
  return self.tabGridViewController;
}

- (void)dismissWithNewTabAnimationToBrowser:(Browser*)browser
                          withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
                                    atIndex:(int)position {
  int tabIndex = std::min(position, browser->GetWebStateList()->count());

  TabInsertionBrowserAgent::FromBrowser(browser)->InsertWebState(
      urlLoadParams.web_params, nil, false, tabIndex, false);

  // Tell the delegate to display the tab.
  DCHECK(self.delegate);
  [self.delegate tabSwitcher:self
      shouldFinishWithBrowser:browser
                 focusOmnibox:NO];
}

- (void)setOtrBrowser:(Browser*)browser {
  DCHECK(self.incognitoMediator);
  self.incognitoMediator.browser = browser;
}

@end
