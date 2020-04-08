// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_coordinator.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_mediator.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_ui_handler.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/empty_credentials_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialListCoordinator () <CredentialListUIHandler,
                                         ConfirmationAlertActionHandler>

// Base view controller from where |viewController| is presented.
@property(nonatomic, weak) UIViewController* baseViewController;

// The view controller of this coordinator.
@property(nonatomic, strong) UINavigationController* viewController;

// The mediator of this coordinator.
@property(nonatomic, strong) CredentialListMediator* mediator;

// Interface for the persistent credential store.
@property(nonatomic, weak) id<CredentialStore> credentialStore;

// The extension context in which the credential list was started.
@property(nonatomic, weak) ASCredentialProviderExtensionContext* context;

// The service identifiers to prioritize in a match is found.
@property(nonatomic, strong)
    NSArray<ASCredentialServiceIdentifier*>* serviceIdentifiers;

@end

@implementation CredentialListCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
               credentialStore:(id<CredentialStore>)credentialStore
                       context:(ASCredentialProviderExtensionContext*)context
            serviceIdentifiers:
                (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _context = context;
    _serviceIdentifiers = serviceIdentifiers;
    _credentialStore = credentialStore;
  }
  return self;
}

- (void)start {
  CredentialListViewController* credentialListViewController =
      [[CredentialListViewController alloc] init];
  self.mediator = [[CredentialListMediator alloc]
        initWithConsumer:credentialListViewController
               UIHandler:self
         credentialStore:self.credentialStore
                 context:self.context
      serviceIdentifiers:self.serviceIdentifiers];

  self.viewController = [[UINavigationController alloc]
      initWithRootViewController:credentialListViewController];
  self.viewController.modalPresentationStyle =
      UIModalPresentationCurrentContext;
  [self.baseViewController presentViewController:self.viewController
                                        animated:NO
                                      completion:nil];
  [self.mediator fetchCredentials];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - CredentialListUIHandler

- (void)showEmptyCredentials {
  EmptyCredentialsViewController* emptyCredentialsViewController =
      [[EmptyCredentialsViewController alloc] init];
  emptyCredentialsViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  emptyCredentialsViewController.actionHandler = self;
  [self.viewController.navigationController
      presentViewController:emptyCredentialsViewController
                   animated:NO
                 completion:nil];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDone {
  // Finish the extension. There is no recovery from the empty credentials
  // state.
  NSError* error =
      [[NSError alloc] initWithDomain:ASExtensionErrorDomain
                                 code:ASExtensionErrorCodeUserCanceled
                             userInfo:nil];
  [self.context cancelRequestWithError:error];
}

- (void)confirmationAlertPrimaryAction {
  // No-op.
}

- (void)confirmationAlertLearnMoreAction {
  // No-op.
}

@end
