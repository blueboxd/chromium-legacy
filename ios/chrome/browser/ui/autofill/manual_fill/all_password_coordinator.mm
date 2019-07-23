// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/all_password_coordinator.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_navigator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_animator.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace manual_fill {

NSString* const kPasswordDoneButtonAccessibilityIdentifier =
    @"kManualFillPasswordDoneButtonAccessibilityIdentifier";

}  // namespace manual_fill

@interface ManualFillAllPasswordCoordinator () <
    UIViewControllerTransitioningDelegate>

// Fetches and filters the passwords for the view controller.
@property(nonatomic, strong) ManualFillPasswordMediator* passwordMediator;

// The view controller presented above the keyboard where the user can select
// one of their passwords.
@property(nonatomic, strong) PasswordViewController* passwordViewController;

@end

@implementation ManualFillAllPasswordCoordinator

// Property tagged dynamic because it overrides super class delegate with and
// extension of the super delegate type (i.e. PasswordCoordinatorDelegate
// extends FallbackCoordinatorDelegate).
@dynamic delegate;

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                  browserState:(ios::ChromeBrowserState*)browserState
              injectionHandler:(ManualFillInjectionHandler*)injectionHandler
                     navigator:(id<PasswordListNavigator>)navigator {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState
                          injectionHandler:injectionHandler];
  if (self) {
    UISearchController* searchController =
        [[UISearchController alloc] initWithSearchResultsController:nil];

    _passwordViewController = [[PasswordViewController alloc]
        initWithSearchController:searchController];
    _passwordViewController.contentInsetsAlwaysEqualToSafeArea = YES;

    auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
        browserState, ServiceAccessType::EXPLICIT_ACCESS);
    FaviconLoader* faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(self.browserState);

    _passwordMediator = [[ManualFillPasswordMediator alloc]
        initWithPasswordStore:passwordStore
                faviconLoader:faviconLoader];

    [_passwordMediator fetchPasswordsForURL:GURL::EmptyGURL()];
    _passwordMediator.actionSectionEnabled = NO;
    _passwordMediator.consumer = _passwordViewController;
    _passwordMediator.contentDelegate = injectionHandler;
    _passwordMediator.navigator = navigator;

    _passwordViewController.imageDataSource = _passwordMediator;

    searchController.searchResultsUpdater = _passwordMediator;

    UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                             target:navigator
                             action:@selector(dismissPresentedViewController)];
    doneButton.accessibilityIdentifier =
        manual_fill::kPasswordDoneButtonAccessibilityIdentifier;
    _passwordViewController.navigationItem.rightBarButtonItem = doneButton;
  }
  return self;
}

- (void)start {
  [super start];
  TableViewNavigationController* navigationController =
      [[TableViewNavigationController alloc]
          initWithTable:self.passwordViewController];
  navigationController.transitioningDelegate = self;
  [navigationController setModalPresentationStyle:UIModalPresentationCustom];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.passwordViewController dismissViewControllerAnimated:NO completion:nil];
  [super stop];
}

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  TableViewPresentationController* presentationController =
      [[TableViewPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  return presentationController;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  UITraitCollection* traitCollection = presenting.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  TableViewAnimator* animator = [[TableViewAnimator alloc] init];
  animator.presenting = YES;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  UITraitCollection* traitCollection = dismissed.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  TableViewAnimator* animator = [[TableViewAnimator alloc] init];
  animator.presenting = NO;
  return animator;
}

#pragma mark - FallbackCoordinator

- (UIViewController*)viewController {
  return self.passwordViewController;
}

@end
