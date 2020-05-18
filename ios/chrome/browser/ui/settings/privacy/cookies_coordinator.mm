// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/cookies_coordinator.h"

#include "base/logging.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_commands.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_mediator.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrivacyCookiesCoordinator () <
    PrivacyCookiesViewControllerPresentationDelegate>

@property(nonatomic, strong) PrivacyCookiesViewController* viewController;
@property(nonatomic, strong) PrivacyCookiesMediator* mediator;

@end

@implementation PrivacyCookiesCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ([super initWithBaseViewController:navigationController browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewController = [[PrivacyCookiesViewController alloc]
      initWithStyle:UITableViewStylePlain];

  DCHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  self.viewController.presentationDelegate = self;

  self.mediator = [[PrivacyCookiesMediator alloc]
      initWithPrefService:self.browser->GetBrowserState()->GetPrefs()
              settingsMap:ios::HostContentSettingsMapFactory::
                              GetForBrowserState(
                                  self.browser->GetBrowserState())];
  self.mediator.consumer = self.viewController;
  self.viewController.handler = self.mediator;
}

- (void)stop {
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - PrivacyCookiesViewControllerPresentationDelegate

- (void)privacyCookiesViewControllerWasRemoved:
    (PrivacyCookiesViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate privacyCookiesCoordinatorViewControllerWasRemoved:self];
}

@end
