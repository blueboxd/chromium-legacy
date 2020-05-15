// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_coordinator.h"

#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_mediator.h"
#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageInfoCoordinator ()

@property(nonatomic, strong)
    TableViewNavigationController* navigationController;
@property(nonatomic, strong) CommandDispatcher* dispatcher;
@property(nonatomic, strong) PageInfoViewController* viewController;

@end

@implementation PageInfoCoordinator

@synthesize presentationProvider = _presentationProvider;

#pragma mark - ChromeCoordinator

- (void)start {
  self.dispatcher = self.browser->GetCommandDispatcher();

  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  web::NavigationItem* navItem =
      webState->GetNavigationManager()->GetVisibleItem();

  bool offlinePage =
      OfflinePageTabHelper::FromWebState(webState)->presenting_offline_page();

  PageInfoSiteSecurityDescription* description =
      [PageInfoSiteSecurityMediator configurationForURL:navItem->GetURL()
                                              SSLStatus:navItem->GetSSL()
                                            offlinePage:offlinePage];

  self.viewController = [[PageInfoViewController alloc]
      initWithSiteSecurityDescription:description];
  self.navigationController =
      [[TableViewNavigationController alloc] initWithTable:self.viewController];

  self.dispatcher = self.browser->GetCommandDispatcher();
  self.viewController.handler =
      HandlerForProtocol(self.dispatcher, BrowserCommands);

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.baseViewController.presentedViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.navigationController = nil;
  self.viewController = nil;
}

@end
