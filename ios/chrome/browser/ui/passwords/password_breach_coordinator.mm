// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_coordinator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/password_breach_commands.h"
#import "ios/chrome/browser/ui/passwords/password_breach_learn_more_view_controller.h"
#import "ios/chrome/browser/ui/passwords/password_breach_mediator.h"
#import "ios/chrome/browser/ui/passwords/password_breach_presenter.h"
#import "ios/chrome/browser/ui/passwords/password_breach_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordBreachCoordinator () <PasswordBreachCommands,
                                         PasswordBreachPresenter>

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordBreachViewController* viewController;

// Learn more view controller, when presented.
@property(nonatomic, strong)
    PasswordBreachLearnMoreViewController* learnMoreViewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordBreachMediator* mediator;

@end

@implementation PasswordBreachCoordinator

- (void)start {
  [super start];
  // To start, a mediator and view controller should be ready.
  DCHECK(self.viewController);
  DCHECK(self.mediator);
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  [super stop];
}

#pragma mark - PasswordBreachCommands

- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType
                                  URL:(const GURL&)URL {
  DCHECK(self.browser);
  self.viewController = [[PasswordBreachViewController alloc] init];
  self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  if (@available(iOS 13, *)) {
    self.viewController.modalInPresentation = YES;
  }
  id<ApplicationCommands> dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  self.mediator =
      [[PasswordBreachMediator alloc] initWithConsumer:self.viewController
                                             presenter:self
                                            dispatcher:dispatcher
                                                   URL:URL
                                              leakType:leakType];
  self.viewController.actionHandler = self.mediator;
  [self start];
}

#pragma mark - PasswordBreachPresenter

- (void)presentLearnMore {
  self.learnMoreViewController =
      [[PasswordBreachLearnMoreViewController alloc] initWithPresenter:self];
  [self.viewController presentViewController:self.learnMoreViewController
                                    animated:YES
                                  completion:nil];
  self.learnMoreViewController.popoverPresentationController.barButtonItem =
      self.viewController.helpButton;
  self.learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;
}

- (void)dismissLearnMore {
  [self.learnMoreViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.learnMoreViewController = nil;
}

@end
