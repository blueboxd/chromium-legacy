// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

#include "base/ios/block_types.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/ui/commands/application_commands.h"
#include "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_transitioning_delegate.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RecentTabsCoordinator ()<RecentTabsPresentationDelegate>
// Completion block called once the recentTabsViewController is dismissed.
@property(nonatomic, copy) ProceduralBlock completion;
// Mediator being managed by this Coordinator.
@property(nonatomic, strong) RecentTabsMediator* mediator;
// ViewController being managed by this Coordinator.
@property(nonatomic, strong)
    TableViewNavigationController* recentTabsNavigationController;
@property(nonatomic, strong)
    RecentTabsTransitioningDelegate* recentTabsTransitioningDelegate;
@end

@implementation RecentTabsCoordinator
@synthesize completion = _completion;
@synthesize mediator = _mediator;
@synthesize recentTabsNavigationController = _recentTabsNavigationController;
@synthesize recentTabsTransitioningDelegate = _recentTabsTransitioningDelegate;

- (void)start {
  // Initialize and configure RecentTabsTableViewController.
  RecentTabsTableViewController* recentTabsTableViewController =
      [[RecentTabsTableViewController alloc] init];
  recentTabsTableViewController.browser = self.browser;
  recentTabsTableViewController.loadStrategy = self.loadStrategy;
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<ApplicationCommands> handler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  recentTabsTableViewController.handler = handler;
  recentTabsTableViewController.presentationDelegate = self;

  // Adds the "Done" button and hooks it up to |stop|.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissButtonTapped)];
  [dismissButton
      setAccessibilityIdentifier:kTableViewNavigationDismissButtonId];
  recentTabsTableViewController.navigationItem.rightBarButtonItem =
      dismissButton;

  // Initialize and configure RecentTabsMediator. Make sure to use the
  // OriginalChromeBrowserState since the mediator services need a SignIn
  // manager which is not present in an OffTheRecord BrowserState.
  DCHECK(!self.mediator);
  self.mediator = [[RecentTabsMediator alloc] init];
  self.mediator.browserState =
      self.browser->GetBrowserState()->GetOriginalChromeBrowserState();
  // Set the consumer first before calling [self.mediator initObservers] and
  // then [self.mediator configureConsumer].
  self.mediator.consumer = recentTabsTableViewController;
  // TODO(crbug.com/845636) : Currently, the image data source must be set
  // before the mediator starts updating its consumer. Fix this so that order of
  // calls does not matter.
  recentTabsTableViewController.imageDataSource = self.mediator;
  recentTabsTableViewController.delegate = self.mediator;
  [self.mediator initObservers];
  [self.mediator configureConsumer];

  // Present RecentTabsNavigationController.
  self.recentTabsNavigationController = [[TableViewNavigationController alloc]
      initWithTable:recentTabsTableViewController];
  self.recentTabsNavigationController.toolbarHidden = YES;

  BOOL useCustomPresentation = YES;
  if (IsCollectionsCardPresentationStyleEnabled()) {
    if (@available(iOS 13, *)) {
      [self.recentTabsNavigationController
          setModalPresentationStyle:UIModalPresentationFormSheet];
      self.recentTabsNavigationController.presentationController.delegate =
          recentTabsTableViewController;
      useCustomPresentation = NO;
    }
  }

  if (useCustomPresentation) {
    self.recentTabsTransitioningDelegate =
        [[RecentTabsTransitioningDelegate alloc] init];
    self.recentTabsNavigationController.transitioningDelegate =
        self.recentTabsTransitioningDelegate;
    [self.recentTabsNavigationController
        setModalPresentationStyle:UIModalPresentationCustom];
  }

  recentTabsTableViewController.preventUpdates = NO;

  [self.baseViewController
      presentViewController:self.recentTabsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  // TODO(crbug.com/805135): Create RecentTabsLocalCommands?. Remove
  // "base/mac/foundation_util.h" import then.
  RecentTabsTableViewController* recentTabsTableViewController =
      base::mac::ObjCCastStrict<RecentTabsTableViewController>(
          self.recentTabsNavigationController.tableViewController);
  [recentTabsTableViewController dismissModals];
  [self.recentTabsNavigationController
      dismissViewControllerAnimated:YES
                         completion:self.completion];
  self.recentTabsNavigationController = nil;
  self.recentTabsTransitioningDelegate = nil;
  [self.mediator disconnect];
}

- (void)dismissButtonTapped {
  base::RecordAction(base::UserMetricsAction("MobileRecentTabsClose"));
  [self stop];
}

#pragma mark - RecentTabsPresentationDelegate

- (void)dismissRecentTabs {
  self.completion = nil;
  [self stop];
}

- (void)showActiveRegularTabFromRecentTabs {
  // Stopping this coordinator reveals the tab UI underneath.
  self.completion = nil;
  [self stop];
}

- (void)showHistoryFromRecentTabs {
  // Dismiss recent tabs before presenting history.
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<ApplicationCommands> handler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  __weak RecentTabsCoordinator* weakSelf = self;
  self.completion = ^{
    [handler showHistory];
    weakSelf.completion = nil;
  };
  [self stop];
}

@end
