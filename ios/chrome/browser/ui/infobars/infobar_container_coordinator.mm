// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_coordinator.h"

#include <memory>

#import "base/mac/foundation_util.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"
#import "ios/chrome/browser/ui/infobars/infobar_container_consumer.h"
#include "ios/chrome/browser/ui/infobars/infobar_container_mediator.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"
#include "ios/chrome/browser/ui/infobars/legacy_infobar_container_view_controller.h"
#include "ios/chrome/browser/upgrade/upgrade_center.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarContainerCoordinator () <InfobarContainer,
                                           InfobarContainerConsumer>

@property(nonatomic, assign) WebStateList* webStateList;

// ViewController of the Infobar currently being presented, can be nil.
@property(nonatomic, weak) UIViewController* infobarViewController;
// UIViewController that contains legacy Infobars.
@property(nonatomic, strong)
    LegacyInfobarContainerViewController* legacyContainerViewController;
// The mediator for this Coordinator.
@property(nonatomic, strong) InfobarContainerMediator* mediator;
// The dispatcher for this Coordinator.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;
// If YES the legacyContainer Fullscreen support will be disabled.
// TODO(crbug.com/927064): Remove this once the legacy container is no longer
// needed.
@property(nonatomic, assign) BOOL legacyContainerFullscrenSupportDisabled;

@end

@implementation InfobarContainerCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _webStateList = webStateList;
  }
  return self;
}

- (void)start {
  DCHECK(self.positioner);
  DCHECK(self.dispatcher);

  // Creates the LegacyInfobarContainerVC.
  LegacyInfobarContainerViewController* legacyContainer =
      [[LegacyInfobarContainerViewController alloc]
          initWithFullscreenController:
              FullscreenControllerFactory::GetInstance()->GetForBrowserState(
                  self.browserState)];
  [self.baseViewController addChildViewController:legacyContainer];
  // TODO(crbug.com/892376): Shouldn't modify the BaseVC hierarchy, BVC
  // needs to handle this.
  [self.baseViewController.view insertSubview:legacyContainer.view
                                 aboveSubview:self.positioner.parentView];
  [legacyContainer didMoveToParentViewController:self.baseViewController];
  legacyContainer.positioner = self.positioner;
  legacyContainer.disableFullscreenSupport =
      self.legacyContainerFullscrenSupportDisabled;
  self.legacyContainerViewController = legacyContainer;

  // Creates the mediator using both consumers.
  self.mediator = [[InfobarContainerMediator alloc]
      initWithConsumer:self
        legacyConsumer:self.legacyContainerViewController
          webStateList:self.webStateList];

  self.mediator.syncPresenter = self.syncPresenter;

  [[UpgradeCenter sharedInstance] registerClient:self.mediator
                                  withDispatcher:self.dispatcher];
}

- (void)stop {
  [[UpgradeCenter sharedInstance] unregisterClient:self.mediator];
  [self.mediator disconnect];
  if (!self.legacyContainerViewController)
    return;

  [self.legacyContainerViewController willMoveToParentViewController:nil];
  [self.legacyContainerViewController.view removeFromSuperview];
  [self.legacyContainerViewController removeFromParentViewController];
  self.legacyContainerViewController = nil;
}

#pragma mark - Public Interface

- (void)hideContainer:(BOOL)hidden {
  [self.legacyContainerViewController.view setHidden:hidden];
  [self.infobarViewController.view setHidden:hidden];
}

- (UIView*)legacyContainerView {
  return self.legacyContainerViewController.view;
}

- (void)updateInfobarContainer {
  // TODO(crbug.com/927064): No need to update the non legacy container since
  // updateLayoutAnimated is NO-OP.
  [self.legacyContainerViewController updateLayoutAnimated:NO];
}

- (BOOL)isInfobarPresentingForWebState:(web::WebState*)webState {
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  if (infoBarManager->infobar_count() > 0) {
    return YES;
  }
  return NO;
}

- (void)dismissInfobarBannerAnimated:(BOOL)animated
                          completion:(void (^)())completion {
  DCHECK(IsInfobarUIRebootEnabled());
  InfobarCoordinator* infobarCoordinator =
      static_cast<InfobarCoordinator*>(self.activeChildCoordinator);
  [infobarCoordinator dismissInfobarBannerAnimated:animated
                                        completion:completion];
}

#pragma mark - Accessors

- (void)setCommandDispatcher:(CommandDispatcher*)commandDispatcher {
  if (commandDispatcher == self.commandDispatcher) {
    return;
  }

  if (self.commandDispatcher) {
    [self.commandDispatcher stopDispatchingToTarget:self];
  }

  [commandDispatcher startDispatchingToTarget:self
                                  forSelector:@selector(displayModalInfobar)];
  _commandDispatcher = commandDispatcher;
  self.dispatcher = static_cast<id<ApplicationCommands>>(_commandDispatcher);
}

- (InfobarBannerPresentationState)infobarBannerState {
  DCHECK(IsInfobarUIRebootEnabled());
  InfobarCoordinator* infobarCoordinator =
      static_cast<InfobarCoordinator*>(self.activeChildCoordinator);
  return infobarCoordinator.infobarBannerState;
}

#pragma mark - InfobarConsumer

- (void)addInfoBarWithDelegate:(id<InfobarUIDelegate>)infoBarDelegate {
  DCHECK(IsInfobarUIRebootEnabled());
  InfobarCoordinator* infobarCoordinator =
      static_cast<InfobarCoordinator*>(infoBarDelegate);

  // Present the InfobarBanner, and set the Coordinator and View hierarchies.
  [infobarCoordinator start];
  infobarCoordinator.badgeDelegate = self.mediator;
  infobarCoordinator.browserState = self.browserState;
  infobarCoordinator.baseViewController = self.baseViewController;
  infobarCoordinator.dispatcher = self.dispatcher;
  infobarCoordinator.infobarContainer = self;
  if (!infobarCoordinator.bannerWasPresented)
    [infobarCoordinator presentInfobarBannerAnimated:YES completion:nil];
  self.infobarViewController = infobarCoordinator.bannerViewController;
  [self.childCoordinators addObject:infobarCoordinator];

  // Dismisses the presented InfobarCoordinator banner after
  // kInfobarBannerPresentationDurationInSeconds seconds.
  if (!UIAccessibilityIsVoiceOverRunning()) {
    dispatch_time_t popTime = dispatch_time(
        DISPATCH_TIME_NOW,
        kInfobarBannerPresentationDurationInSeconds * NSEC_PER_SEC);
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void) {
      [infobarCoordinator dismissInfobarBannerAfterInteraction];
    });
  }
}

- (void)setUserInteractionEnabled:(BOOL)enabled {
  [self.infobarViewController.view setUserInteractionEnabled:enabled];
}

- (void)updateLayoutAnimated:(BOOL)animated {
  DCHECK(IsInfobarUIRebootEnabled());
  // TODO(crbug.com/927064): NO-OP - This shouldn't be needed in the new UI
  // since we use autolayout for the contained Infobars.
}

#pragma mark InfobarContainer

- (void)childCoordinatorStopped {
  DCHECK(IsInfobarUIRebootEnabled());
  // TODO(crbug.com/961343): When more than one InfobarCoordinator can exist
  // concurrently, delete only the one that stopped.
  [self.childCoordinators removeAllObjects];
}

#pragma mark - InfobarCommands

- (void)displayModalInfobar {
  InfobarCoordinator* infobarCoordinator =
      static_cast<InfobarCoordinator*>(self.activeChildCoordinator);
  [infobarCoordinator presentInfobarModal];
}

@end
