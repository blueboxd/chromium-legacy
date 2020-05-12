// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_coordinator.h"

#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_mediator.h"
#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageInfoCoordinator ()

@property(nonatomic, strong) CommandDispatcher* dispatcher;
@property(nonatomic, strong) PageInfoViewController* viewController;
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation PageInfoCoordinator

@synthesize presentationProvider = _presentationProvider;
@synthesize navigationController = _navigationController;

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewController =
      [[PageInfoViewController alloc] initWithStyle:UITableViewStylePlain];

  self.navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];

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
