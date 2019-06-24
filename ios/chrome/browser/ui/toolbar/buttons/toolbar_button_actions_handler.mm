// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_actions_handler.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarButtonActionsHandler

- (void)backAction {
  [self.dispatcher goBack];
}

- (void)forwardAction {
  [self.dispatcher goForward];
}

- (void)tabGridTouchDown {
  [self.dispatcher prepareTabSwitcher];
}

- (void)tabGridTouchUp {
  [self.dispatcher displayTabSwitcher];
}

- (void)toolsMenuAction {
  [self.dispatcher showToolsMenuPopup];
}

- (void)shareAction {
  [self.dispatcher sharePage];
}

- (void)reloadAction {
  [self.dispatcher reload];
}

- (void)stopAction {
  [self.dispatcher stopLoading];
}

- (void)bookmarkAction {
  [self.dispatcher bookmarkPage];
}

- (void)searchAction {
  [self.dispatcher closeFindInPage];
  if (base::FeatureList::IsEnabled(kToolbarNewTabButton)) {
    web::WebState::CreateParams params(self.browserState);
    std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

    GURL newTabURL(kChromeUINewTabURL);
    web::NavigationManager::WebLoadParams loadParams(newTabURL);
    loadParams.transition_type = ui::PAGE_TRANSITION_TYPED;
    webState->GetNavigationManager()->LoadURLWithParams(loadParams);

    self.webStateList->InsertWebState(
        self.webStateList->count(), std::move(webState),
        (WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE),
        WebStateOpener());
  } else {
    [self.dispatcher focusOmniboxFromSearchButton];
  }
}

- (void)cancelOmniboxFocusAction {
  [self.dispatcher cancelOmniboxEdit];
}

@end
