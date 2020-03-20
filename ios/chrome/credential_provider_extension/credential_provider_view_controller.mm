// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/credential_provider_view_controller.h"

#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialProviderViewController () <SuccessfulReauthTimeAccessor>

// List coordinator that shows the list of passwords when started.
@property(nonatomic, strong) CredentialListCoordinator* listCoordinator;

// Date kept for ReauthenticationModule.
@property(nonatomic, strong) NSDate* lastSuccessfulReauthTime;

// Reauthentication Module used for reauthentication.
@property(nonatomic, strong) ReauthenticationModule* reauthenticationModule;

// Interface for |reauthenticationModule|, handling mostly the case when no
// hardware for authentication is available.
@property(nonatomic, strong) ReauthenticationHandler* reauthenticationHandler;

@end

@implementation CredentialProviderViewController

- (void)prepareCredentialListForServiceIdentifiers:
    (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers {
  self.listCoordinator = [[CredentialListCoordinator alloc]
      initWithBaseViewController:self
                         context:self.extensionContext
              serviceIdentifiers:serviceIdentifiers];
  [self.listCoordinator start];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // TODO(crbug.com/1045455): Remove, this is for testing purpose only.
  [self reauthenticateIfNeededWithCompletionHandler:^(
            ReauthenticationResult result) {
    // TODO(crbug.com/1045455): Check result before cancelling.
    [self.extensionContext
        cancelRequestWithError:
            [[NSError alloc]
                initWithDomain:ASExtensionErrorDomain
                          code:ASExtensionErrorCode::ASExtensionErrorCodeFailed
                      userInfo:nil]];
  }];
}

#pragma mark - Properties

- (ReauthenticationHandler*)reauthenticationHandler {
  if (!self.reauthenticationModule) {
    self.reauthenticationModule = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:self];
    self.reauthenticationHandler = [[ReauthenticationHandler alloc]
        initWithReauthenticationModule:self.reauthenticationModule];
  }
  return _reauthenticationHandler;
}

#pragma mark - Private

- (void)reauthenticateIfNeededWithCompletionHandler:
    (void (^)(ReauthenticationResult))completionHandler {
  [self.reauthenticationHandler
      verifyUserWithCompletionHandler:completionHandler
      presentReminderOnViewController:self];
}

#pragma mark - SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  self.lastSuccessfulReauthTime = [[NSDate alloc] init];
}

@end
