// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_coordinator.h"

#include "base/logging.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/page_info/page_info_cookies_view_controller.h"
#import "ios/chrome/browser/ui/page_info/page_info_mediator.h"
#import "ios/chrome/browser/ui/page_info/page_info_navigation_commands.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_mediator.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_view_controller.h"
#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageInfoCoordinator () <PageInfoNavigationCommands>

@property(nonatomic, strong) CommandDispatcher* dispatcher;
@property(nonatomic, strong) PageInfoViewController* viewController;
@property(nonatomic, strong) PageInfoMediator* mediator;
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation PageInfoCoordinator

@synthesize presentationProvider = _presentationProvider;
@synthesize navigationController = _navigationController;

#pragma mark - ChromeCoordinator

- (void)start {
  self.dispatcher = self.browser->GetCommandDispatcher();
  [self.dispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(PageInfoNavigationCommands)];

  self.viewController =
      [[PageInfoViewController alloc] initWithStyle:UITableViewStylePlain];
  self.viewController.handler =
      HandlerForProtocol(self.dispatcher, PageInfoNavigationCommands);

  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  self.mediator = [[PageInfoMediator alloc] initWithWebState:webState];
  self.mediator.consumer = self.viewController;

  self.navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.dispatcher stopDispatchingToTarget:self];
  [self.baseViewController.presentedViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.dispatcher = nil;
  self.navigationController = nil;
  self.mediator.consumer = nil;
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - PageInfoNavigationCommands

- (void)showSiteSecurityInfo {
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

  PageInfoSiteSecurityViewController* viewController =
      [[PageInfoSiteSecurityViewController alloc]
          initWitDescription:description];

  viewController.handler = HandlerForProtocol(self.dispatcher, BrowserCommands);
  [self.navigationController pushViewController:viewController animated:YES];
}

- (void)showCookiesInfo {
  PageInfoCookiesViewController* viewController =
      [[PageInfoCookiesViewController alloc]
          initWithStyle:UITableViewStylePlain];
  [self.navigationController pushViewController:viewController animated:YES];
}

@end
