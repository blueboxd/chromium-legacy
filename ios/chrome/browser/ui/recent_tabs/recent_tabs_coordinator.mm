// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

#include "base/ios/block_types.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/ui/commands/application_commands.h"
#include "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_provider.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_transitioning_delegate.h"
#include "ios/chrome/browser/ui/recent_tabs/synced_sessions.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RecentTabsCoordinator () <RecentTabsMenuProvider,
                                     RecentTabsPresentationDelegate>
// Completion block called once the recentTabsViewController is dismissed.
@property(nonatomic, copy) ProceduralBlock completion;
// Mediator being managed by this Coordinator.
@property(nonatomic, strong) RecentTabsMediator* mediator;
// ViewController being managed by this Coordinator.
@property(nonatomic, strong)
    TableViewNavigationController* recentTabsNavigationController;
@property(nonatomic, strong)
    RecentTabsTransitioningDelegate* recentTabsTransitioningDelegate;
@property(nonatomic, strong)
    RecentTabsTableViewController* recentTabsTableViewController;

@end

@implementation RecentTabsCoordinator
@synthesize completion = _completion;
@synthesize mediator = _mediator;
@synthesize recentTabsNavigationController = _recentTabsNavigationController;
@synthesize recentTabsTransitioningDelegate = _recentTabsTransitioningDelegate;

- (void)start {
  // Initialize and configure RecentTabsTableViewController.
  self.recentTabsTableViewController =
      [[RecentTabsTableViewController alloc] init];
  self.recentTabsTableViewController.browser = self.browser;
  self.recentTabsTableViewController.loadStrategy = self.loadStrategy;
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<ApplicationCommands> handler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  self.recentTabsTableViewController.handler = handler;
  self.recentTabsTableViewController.presentationDelegate = self;

  if (@available(iOS 13.0, *)) {
    self.recentTabsTableViewController.menuProvider = self;
  }

  // Adds the "Done" button and hooks it up to |stop|.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissButtonTapped)];
  [dismissButton
      setAccessibilityIdentifier:kTableViewNavigationDismissButtonId];
  self.recentTabsTableViewController.navigationItem.rightBarButtonItem =
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
  self.mediator.consumer = self.recentTabsTableViewController;
  // TODO(crbug.com/845636) : Currently, the image data source must be set
  // before the mediator starts updating its consumer. Fix this so that order of
  // calls does not matter.
  self.recentTabsTableViewController.imageDataSource = self.mediator;
  self.recentTabsTableViewController.delegate = self.mediator;
  [self.mediator initObservers];
  [self.mediator configureConsumer];

  // Present RecentTabsNavigationController.
  self.recentTabsNavigationController = [[TableViewNavigationController alloc]
      initWithTable:self.recentTabsTableViewController];
  self.recentTabsNavigationController.toolbarHidden = YES;

  BOOL useCustomPresentation = YES;
  if (IsCollectionsCardPresentationStyleEnabled()) {
    if (@available(iOS 13, *)) {
      [self.recentTabsNavigationController
          setModalPresentationStyle:UIModalPresentationFormSheet];
      self.recentTabsNavigationController.presentationController.delegate =
          self.recentTabsTableViewController;
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

  self.recentTabsTableViewController.preventUpdates = NO;

  [self.baseViewController
      presentViewController:self.recentTabsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [self.recentTabsTableViewController dismissModals];
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

#pragma mark - Private

// Opens all tabs from the given |sectionIdentifier|.
- (void)openAllTabsFromSessionSectionIdentitifer:(NSInteger)sectionIdentifier {
  synced_sessions::DistantSession const* session =
      [self.recentTabsTableViewController
          sessionForSectionIdentifier:sectionIdentifier];
  [self openAllTabsFromSession:session];
}

#pragma mark - RecentTabsPresentationDelegate

- (void)openAllTabsFromSession:(const synced_sessions::DistantSession*)session {
  base::RecordAction(base::UserMetricsAction(
      "MobileRecentTabManagerOpenAllTabsFromOtherDevice"));
  base::UmaHistogramCounts100(
      "Mobile.RecentTabsManager.TotalTabsFromOtherDevicesOpenAll",
      session->tabs.size());

  for (auto const& tab : session->tabs) {
    UrlLoadParams params = UrlLoadParams::InNewTab(tab->virtual_url);
    params.SetInBackground(YES);
    params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    params.load_strategy = self.loadStrategy;
    params.in_incognito = self.browser->GetBrowserState()->IsOffTheRecord();
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  }

  [self showActiveRegularTabFromRecentTabs];
}

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

#pragma mark - RecentTabsMenuProvider

- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
    (TableViewURLItem*)item API_AVAILABLE(ios(13.0)) {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider = ^(
      NSArray<UIMenuElement*>* suggestedActions) {
    if (!weakSelf) {
      // Return an empty menu.
      return [UIMenu menuWithTitle:@"" children:@[]];
    }

    RecentTabsCoordinator* strongSelf = weakSelf;

    // Record that this context menu was shown to the user.
    RecordMenuShown(MenuScenario::kRecentTabsEntry);

    ActionFactory* actionFactory =
        [[ActionFactory alloc] initWithBrowser:strongSelf.browser
                                      scenario:MenuScenario::kRecentTabsEntry];

    NSMutableArray<UIMenuElement*>* menuElements =
        [[NSMutableArray alloc] init];

    [menuElements addObject:[actionFactory actionToOpenInNewTabWithURL:item.URL
                                                            completion:^{
                                                              [strongSelf stop];
                                                            }]];

    [menuElements
        addObject:[actionFactory actionToOpenInNewIncognitoTabWithURL:item.URL
                                                           completion:^{
                                                             [strongSelf stop];
                                                           }]];

    if (IsMultipleScenesSupported()) {
      [menuElements
          addObject:
              [actionFactory
                  actionToOpenInNewWindowWithURL:item.URL
                                  activityOrigin:WindowActivityRecentTabsOrigin
                                      completion:^{
                                        [strongSelf stop];
                                      }]];
    }

    [menuElements addObject:[actionFactory actionToCopyURL:item.URL]];

    return [UIMenu menuWithTitle:@"" children:menuElements];
  };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (UIContextMenuConfiguration*)
    contextMenuConfigurationForHeaderWithSectionIdentifier:
        (NSInteger)sectionIdentifier API_AVAILABLE(ios(13.0)) {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        if (!weakSelf || ![weakSelf.recentTabsTableViewController
                             isSessionSectionIdentifier:sectionIdentifier]) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        // Record that this context menu was shown to the user.
        RecordMenuShown(MenuScenario::kRecentTabsHeader);

        ActionFactory* actionFactory = [[ActionFactory alloc]
            initWithBrowser:weakSelf.browser
                   scenario:MenuScenario::kRecentTabsHeader];

        NSMutableArray<UIMenuElement*>* menuElements =
            [[NSMutableArray alloc] init];

        synced_sessions::DistantSession const* session =
            [weakSelf.recentTabsTableViewController
                sessionForSectionIdentifier:sectionIdentifier];

        if (!session->tabs.empty()) {
          [menuElements
              addObject:[actionFactory actionToOpenAllTabsWithBlock:^{
                [weakSelf
                    openAllTabsFromSessionSectionIdentitifer:sectionIdentifier];
              }]];
        }

        [menuElements
            addObject:[actionFactory actionToHideWithBlock:^{
              [weakSelf.recentTabsTableViewController
                  removeSessionAtSessionSectionIdentifier:sectionIdentifier];
            }]];

        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

@end
