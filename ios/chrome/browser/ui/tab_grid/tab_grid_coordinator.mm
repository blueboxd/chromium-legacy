// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/history/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/legacy_tab_grid_transition_handler.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_adaptor.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/tab_grid_transition_handler.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridCoordinator ()<TabPresentationDelegate,
                                 HistoryPresentationDelegate,
                                 RecentTabsPresentationDelegate>
// Superclass property specialized for the class that this coordinator uses.
@property(nonatomic, weak) TabGridViewController* baseViewController;
// Pointer to the masking view used to prevent the main view controller from
// being shown at launch.
@property(nonatomic, strong) UIView* launchMaskView;
// Commad dispatcher used while this coordinator's view controller is active.
// (for compatibility with the TabSwitcher protocol).
@property(nonatomic, strong) CommandDispatcher* dispatcher;
// Object that internally backs the public  TabSwitcher
@property(nonatomic, strong) TabGridAdaptor* adaptor;
// Container view controller for the BVC to live in; this class's view
// controller will present this.
@property(nonatomic, strong) BVCContainerViewController* bvcContainer;
// Transitioning delegate for the view controller.
@property(nonatomic, strong)
    LegacyTabGridTransitionHandler* legacyTransitionHandler;
// Handler for the transitions between the TabGrid and the Browser.
@property(nonatomic, strong) TabGridTransitionHandler* transitionHandler;
// Mediator for regular Tabs.
@property(nonatomic, strong) TabGridMediator* regularTabsMediator;
// Mediator for incognito Tabs.
@property(nonatomic, strong) TabGridMediator* incognitoTabsMediator;
// Mediator for remote Tabs.
@property(nonatomic, strong) RecentTabsMediator* remoteTabsMediator;
// Coordinator for history, which can be started from recent tabs.
@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;
// YES if the TabViewController has never been shown yet.
@property(nonatomic, assign) BOOL firstPresentation;
@end

@implementation TabGridCoordinator
// Superclass property.
@synthesize baseViewController = _baseViewController;
// Ivars are not auto-synthesized when both accessor and mutator are overridden.
@synthesize regularBrowser = _regularBrowser;
@synthesize incognitoBrowser = _incognitoBrowser;

- (instancetype)initWithWindow:(nullable UIWindow*)window
     applicationCommandEndpoint:
         (id<ApplicationCommands>)applicationCommandEndpoint
    browsingDataCommandEndpoint:
        (id<BrowsingDataCommands>)browsingDataCommandEndpoint {
  if ((self = [super initWithWindow:window])) {
    _dispatcher = [[CommandDispatcher alloc] init];
    [_dispatcher startDispatchingToTarget:applicationCommandEndpoint
                              forProtocol:@protocol(ApplicationCommands)];
    // -startDispatchingToTarget:forProtocol: doesn't pick up protocols the
    // passed protocol conforms to, so ApplicationSettingsCommands and
    // BrowsingDataCommands are explicitly dispatched to the endpoint as well.
    [_dispatcher
        startDispatchingToTarget:applicationCommandEndpoint
                     forProtocol:@protocol(ApplicationSettingsCommands)];
    [_dispatcher startDispatchingToTarget:browsingDataCommandEndpoint
                              forProtocol:@protocol(BrowsingDataCommands)];
  }
  return self;
}

#pragma mark - Public

- (id<TabSwitcher>)tabSwitcher {
  return self.adaptor;
}

- (Browser*)regularBrowser {
  // Ensure browser which is actually used by the mediator is returned, as it
  // may have been updated.
  return self.regularTabsMediator ? self.regularTabsMediator.browser
                                  : _regularBrowser;
}

- (void)setRegularBrowser:(Browser*)regularBrowser {
  if (self.regularTabsMediator) {
    self.regularTabsMediator.browser = regularBrowser;
  } else {
    _regularBrowser = regularBrowser;
  }
}

- (Browser*)incognitoBrowser {
  // Ensure browser which is actually used by the mediator is returned, as it
  // may have been updated.
  return self.incognitoTabsMediator ? self.incognitoTabsMediator.browser
                                    : _incognitoBrowser;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  if (self.incognitoTabsMediator) {
    self.incognitoTabsMediator.browser = incognitoBrowser;
  } else {
    _incognitoBrowser = incognitoBrowser;
  }
}

- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion {
  // Recent tabs context menu may be presented on top of the tab grid.
  [self.baseViewController.remoteTabsViewController dismissModals];
  // History may be presented on top of the tab grid.
  if (self.historyCoordinator) {
    [self.historyCoordinator stopWithCompletion:completion];
  } else if (completion) {
    completion();
  }
}

#pragma mark - ChromeCoordinator

- (void)start {
  TabGridViewController* baseViewController =
      [[TabGridViewController alloc] init];
  baseViewController.handler =
      HandlerForProtocol(self.dispatcher, ApplicationCommands);
  self.legacyTransitionHandler = [[LegacyTabGridTransitionHandler alloc] init];
  self.legacyTransitionHandler.provider = baseViewController;
  baseViewController.modalPresentationStyle = UIModalPresentationCustom;
  baseViewController.transitioningDelegate = self.legacyTransitionHandler;
  baseViewController.tabPresentationDelegate = self;
  _baseViewController = baseViewController;

  self.adaptor = [[TabGridAdaptor alloc] init];
  self.adaptor.tabGridViewController = self.baseViewController;

  self.regularTabsMediator = [[TabGridMediator alloc]
      initWithConsumer:baseViewController.regularTabsConsumer];
  ChromeBrowserState* regularBrowserState =
      _regularBrowser ? _regularBrowser->GetBrowserState() : nullptr;
  WebStateList* regularWebStateList =
      _regularBrowser ? _regularBrowser->GetWebStateList() : nullptr;

  self.regularTabsMediator.browser = _regularBrowser;
  if (regularBrowserState) {
    self.regularTabsMediator.tabRestoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(
            regularBrowserState);
  }
  self.incognitoTabsMediator = [[TabGridMediator alloc]
      initWithConsumer:baseViewController.incognitoTabsConsumer];
  self.incognitoTabsMediator.browser = _incognitoBrowser;
  self.adaptor.incognitoMediator = self.incognitoTabsMediator;
  baseViewController.regularTabsDelegate = self.regularTabsMediator;
  baseViewController.incognitoTabsDelegate = self.incognitoTabsMediator;
  baseViewController.regularTabsImageDataSource = self.regularTabsMediator;
  baseViewController.incognitoTabsImageDataSource = self.incognitoTabsMediator;

  // TODO(crbug.com/845192) : Remove RecentTabsTableViewController dependency on
  // ChromeBrowserState so that we don't need to expose the view controller.
  baseViewController.remoteTabsViewController.browser = self.regularBrowser;
  self.remoteTabsMediator = [[RecentTabsMediator alloc] init];
  self.remoteTabsMediator.browserState = regularBrowserState;
  self.remoteTabsMediator.consumer = baseViewController.remoteTabsConsumer;
  self.remoteTabsMediator.webStateList = regularWebStateList;
  // TODO(crbug.com/845636) : Currently, the image data source must be set
  // before the mediator starts updating its consumer. Fix this so that order of
  // calls does not matter.
  baseViewController.remoteTabsViewController.imageDataSource =
      self.remoteTabsMediator;
  baseViewController.remoteTabsViewController.delegate =
      self.remoteTabsMediator;
  baseViewController.remoteTabsViewController.handler =
      HandlerForProtocol(self.dispatcher, ApplicationCommands);
  baseViewController.remoteTabsViewController.loadStrategy =
      UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB;
  baseViewController.remoteTabsViewController.restoredTabDisposition =
      WindowOpenDisposition::NEW_FOREGROUND_TAB;
  baseViewController.remoteTabsViewController.presentationDelegate = self;

  if (!base::FeatureList::IsEnabled(kContainedBVC)) {
    // Insert the launch screen view in front of this view to hide it until
    // after launch. This should happen before |baseViewController| is made the
    // window's root view controller.
    NSBundle* mainBundle = base::mac::FrameworkBundle();
    NSArray* topObjects = [mainBundle loadNibNamed:@"LaunchScreen"
                                             owner:self
                                           options:nil];
    UIViewController* launchScreenController =
        base::mac::ObjCCastStrict<UIViewController>([topObjects lastObject]);
    self.launchMaskView = launchScreenController.view;
    [baseViewController.view addSubview:self.launchMaskView];
  } else {
    self.firstPresentation = YES;
  }

  // TODO(crbug.com/850387) : Currently, consumer calls from the mediator
  // prematurely loads the view in |RecentTabsTableViewController|. Fix this so
  // that the view is loaded only by an explicit placement in the view
  // hierarchy. As a workaround, the view controller hierarchy is loaded here
  // before |RecentTabsMediator| updates are started.
  self.window.rootViewController = self.baseViewController;
  if (self.remoteTabsMediator.browserState) {
    [self.remoteTabsMediator initObservers];
    [self.remoteTabsMediator refreshSessionsView];
  }

  // Once the mediators are set up, stop keeping pointers to the browsers used
  // to initialize them.
  _regularBrowser = nil;
  _incognitoBrowser = nil;
}

- (void)stop {
  // The TabGridViewController may still message its application commands
  // handler after this coordinator has stopped; make this action a no-op by
  // setting the handler to nil.
  self.baseViewController.handler = nil;
  [self.dispatcher stopDispatchingForProtocol:@protocol(ApplicationCommands)];
  [self.dispatcher
      stopDispatchingForProtocol:@protocol(ApplicationSettingsCommands)];
  [self.dispatcher stopDispatchingForProtocol:@protocol(BrowsingDataCommands)];

  // Disconnect UI from models they observe.
  self.regularTabsMediator.browser = nil;
  self.incognitoTabsMediator.browser = nil;

  // TODO(crbug.com/845192) : RecentTabsTableViewController behaves like a
  // coordinator and that should be factored out.
  [self.baseViewController.remoteTabsViewController dismissModals];
  [self.remoteTabsMediator disconnect];
  self.remoteTabsMediator = nil;
}

#pragma mark - ViewControllerSwapping

- (UIViewController*)activeViewController {
  if (self.bvcContainer) {
    if (!base::FeatureList::IsEnabled(kContainedBVC)) {
      DCHECK_EQ(self.bvcContainer,
                self.baseViewController.presentedViewController);
    }
    DCHECK(self.bvcContainer.currentBVC);
    return self.bvcContainer.currentBVC;
  }
  return self.baseViewController;
}

- (UIViewController*)viewController {
  return self.baseViewController;
}

- (void)prepareToShowTabSwitcher:(id<TabSwitcher>)tabSwitcher {
  DCHECK(tabSwitcher);
  DCHECK_EQ([tabSwitcher viewController], self.baseViewController);
  // No-op if the BVC isn't being presented.
  if (!self.bvcContainer)
    return;
  [base::mac::ObjCCast<TabGridViewController>(self.baseViewController)
      prepareForAppearance];
}

- (void)showTabSwitcher:(id<TabSwitcher>)tabSwitcher {
  DCHECK(tabSwitcher);
  DCHECK_EQ([tabSwitcher viewController], self.baseViewController);
  // It's also expected that |tabSwitcher| will be |self.tabSwitcher|, but that
  // may not be worth a DCHECK?

  BOOL animated = !self.animationsDisabledForTesting;

  // If a BVC is currently being presented, dismiss it.  This will trigger any
  // necessary animations.
  if (self.bvcContainer) {
    if (base::FeatureList::IsEnabled(kContainedBVC)) {
      // This is done with a dispatch to make sure that the view isn't added to
      // the view hierarchy right away, as it is not the expectations of the
      // API.
      dispatch_async(dispatch_get_main_queue(), ^{
        [self.baseViewController contentWillAppearAnimated:animated];
        self.baseViewController.childViewControllerForStatusBarStyle = nil;

        self.transitionHandler = [[TabGridTransitionHandler alloc]
            initWithLayoutProvider:self.baseViewController];
        self.transitionHandler.animationDisabled = !animated;
        [self.transitionHandler
            transitionFromBrowser:self.bvcContainer
                        toTabGrid:self.baseViewController
                   withCompletion:^{
                     self.bvcContainer = nil;
                     [self.baseViewController contentDidAppear];
                   }];
      });
    } else {
      self.bvcContainer.transitioningDelegate = self.legacyTransitionHandler;
      self.bvcContainer = nil;
      [self.baseViewController dismissViewControllerAnimated:animated
                                                  completion:nil];
    }
  }
  // Record when the tab switcher is presented.
  base::RecordAction(base::UserMetricsAction("MobileTabGridEntered"));
}

- (void)showTabViewController:(UIViewController*)viewController
                   completion:(ProceduralBlock)completion {
  DCHECK(viewController);

  // Record when the tab switcher is dismissed.
  base::RecordAction(base::UserMetricsAction("MobileTabGridExited"));

  // If another BVC is already being presented, swap this one into the
  // container.
  if (self.bvcContainer) {
    self.bvcContainer.currentBVC = viewController;
    if (completion) {
      completion();
    }
    return;
  }

  self.bvcContainer = [[BVCContainerViewController alloc] init];
  self.bvcContainer.modalPresentationStyle = UIModalPresentationFullScreen;
  self.bvcContainer.currentBVC = viewController;
  self.bvcContainer.transitioningDelegate = self.legacyTransitionHandler;
  BOOL animated = !self.animationsDisabledForTesting;
  // Never animate the first time.
  if (self.launchMaskView || self.firstPresentation)
    animated = NO;

  // Extened |completion| to signal the tab switcher delegate
  // that the animated "tab switcher dismissal" (that is, presenting something
  // on top of the tab switcher) transition has completed.
  // Finally, the launch mask view should be removed.
  ProceduralBlock extendedCompletion = ^{
    [self.tabSwitcher.delegate
        tabSwitcherDismissTransitionDidEnd:self.tabSwitcher];
    if (completion) {
      completion();
    }
    [self.launchMaskView removeFromSuperview];
    self.launchMaskView = nil;
    self.firstPresentation = NO;
  };

  if (base::FeatureList::IsEnabled(kContainedBVC)) {
    self.baseViewController.childViewControllerForStatusBarStyle =
        self.bvcContainer.currentBVC;

    [self.adaptor.tabGridViewController contentWillDisappearAnimated:animated];

    self.transitionHandler = [[TabGridTransitionHandler alloc]
        initWithLayoutProvider:self.baseViewController];
    self.transitionHandler.animationDisabled = !animated;
    [self.transitionHandler transitionFromTabGrid:self.baseViewController
                                        toBrowser:self.bvcContainer
                                   withCompletion:^{
                                     extendedCompletion();
                                   }];

  } else {
    [self.baseViewController presentViewController:self.bvcContainer
                                          animated:animated
                                        completion:extendedCompletion];
  }
}

#pragma mark - TabPresentationDelegate

- (void)showActiveTabInPage:(TabGridPage)page focusOmnibox:(BOOL)focusOmnibox {
  DCHECK(self.regularBrowser && self.incognitoBrowser);
  Browser* activeBrowser = nullptr;
  switch (page) {
    case TabGridPageIncognitoTabs:
      DCHECK_GT(self.incognitoBrowser->GetWebStateList()->count(), 0);
      activeBrowser = self.incognitoBrowser;
      break;
    case TabGridPageRegularTabs:
      DCHECK_GT(self.regularBrowser->GetWebStateList()->count(), 0);
      activeBrowser = self.regularBrowser;
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "It is invalid to have an active tab in remote tabs.";
      // This appears to come up in release -- see crbug.com/1069243.
      // Defensively early return instead of continuing.
      return;
  }
  // Trigger the transition through the TabSwitcher delegate. This will in turn
  // call back into this coordinator via the ViewControllerSwapping protocol.
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
                 shouldFinishWithBrowser:activeBrowser
                            focusOmnibox:focusOmnibox];
}

#pragma mark - RecentTabsPresentationDelegate

- (void)dismissRecentTabs {
  // It is valid for tab grid to ignore this since recent tabs is embedded and
  // will not be dismissed.
}

- (void)showHistoryFromRecentTabs {
  // A history coordinator from main_controller won't work properly from the
  // tab grid. Using a local coordinator works better and we need to set
  // |loadStrategy| to YES to ALWAYS_NEW_FOREGROUND_TAB.
  self.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.regularBrowser];
  self.historyCoordinator.loadStrategy =
      UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB;
  self.historyCoordinator.presentationDelegate = self;
  [self.historyCoordinator start];
}

- (void)showActiveRegularTabFromRecentTabs {
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
                 shouldFinishWithBrowser:self.regularBrowser
                            focusOmnibox:NO];
}

#pragma mark - HistoryPresentationDelegate

- (void)showActiveRegularTabFromHistory {
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
                 shouldFinishWithBrowser:self.regularBrowser
                            focusOmnibox:NO];
}

- (void)showActiveIncognitoTabFromHistory {
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
                 shouldFinishWithBrowser:self.incognitoBrowser
                            focusOmnibox:NO];
}

@end
