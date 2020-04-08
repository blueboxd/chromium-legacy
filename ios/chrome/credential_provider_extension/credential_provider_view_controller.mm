// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/credential_provider_view_controller.h"

#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialProviderViewController () <SuccessfulReauthTimeAccessor>

// Interface for the persistent credential store.
@property(nonatomic, strong) id<CredentialStore> credentialStore;

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
  [self reauthenticateIfNeededWithCompletionHandler:^(
            ReauthenticationResult result) {
    if (result != ReauthenticationResult::kFailure) {
      self.listCoordinator = [[CredentialListCoordinator alloc]
          initWithBaseViewController:self
                     credentialStore:self.credentialStore
                             context:self.extensionContext
                  serviceIdentifiers:serviceIdentifiers];
      [self.listCoordinator start];

    } else {
      [self.extensionContext
          cancelRequestWithError:
              [[NSError alloc] initWithDomain:ASExtensionErrorDomain
                                         code:ASExtensionErrorCode::
                                                  ASExtensionErrorCodeFailed
                                     userInfo:nil]];
    }
  }];
}

#pragma mark - Properties

- (id<CredentialStore>)credentialStore {
  if (!_credentialStore) {
    _credentialStore = [[ArchivableCredentialStore alloc]
        initWithFileURL:CredentialProviderSharedArchivableStoreURL()];
  }
  return _credentialStore;
}

- (ReauthenticationHandler*)reauthenticationHandler {
  if (!_reauthenticationHandler) {
    _reauthenticationHandler = [[ReauthenticationHandler alloc]
        initWithReauthenticationModule:self.reauthenticationModule];
  }
  return _reauthenticationHandler;
}

- (ReauthenticationModule*)reauthenticationModule {
  if (!_reauthenticationModule) {
    _reauthenticationModule = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:self];
  }
  return _reauthenticationModule;
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
