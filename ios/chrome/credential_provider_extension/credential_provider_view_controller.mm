// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/credential_provider_view_controller.h"

#import <Foundation/Foundation.h>

#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/credential_provider_extension/password_util.h"
#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"
#import "ios/chrome/credential_provider_extension/ui/consent_coordinator.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialProviderViewController () <SuccessfulReauthTimeAccessor>

// Interface for the persistent credential store.
@property(nonatomic, strong) id<CredentialStore> credentialStore;

// List coordinator that shows the list of passwords when started.
@property(nonatomic, strong) CredentialListCoordinator* listCoordinator;

// Consent coordinator that shows a view requesting device auth in order to
// enable the extension.
@property(nonatomic, strong) ConsentCoordinator* consentCoordinator;

// Date kept for ReauthenticationModule.
@property(nonatomic, strong) NSDate* lastSuccessfulReauthTime;

// Reauthentication Module used for reauthentication.
@property(nonatomic, strong) ReauthenticationModule* reauthenticationModule;

// Interface for |reauthenticationModule|, handling mostly the case when no
// hardware for authentication is available.
@property(nonatomic, strong) ReauthenticationHandler* reauthenticationHandler;

@end

@implementation CredentialProviderViewController

#pragma mark - ASCredentialProviderViewController

- (void)prepareCredentialListForServiceIdentifiers:
    (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers {
  [self reauthenticateIfNeededWithCompletionHandler:^(
            ReauthenticationResult result) {
    if (result != ReauthenticationResult::kFailure) {
      self.listCoordinator = [[CredentialListCoordinator alloc]
          initWithBaseViewController:self
                     credentialStore:self.credentialStore
                             context:self.extensionContext
                  serviceIdentifiers:serviceIdentifiers
             reauthenticationHandler:self.reauthenticationHandler];
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

- (void)provideCredentialWithoutUserInteractionForIdentity:
    (ASPasswordCredentialIdentity*)credentialIdentity {
  // reauthenticationModule can't attempt reauth when no password is set. This
  // means a password shouldn't be retrieved.
  if (!self.reauthenticationModule.canAttemptReauth) {
    NSError* error = [[NSError alloc]
        initWithDomain:ASExtensionErrorDomain
                  code:ASExtensionErrorCodeUserInteractionRequired
              userInfo:nil];
    [self.extensionContext cancelRequestWithError:error];
    return;
  }
  // iOS already gates the password with device auth for
  // -provideCredentialWithoutUserInteractionForIdentity:. Not using
  // reauthenticationModule here to avoid a double authentication request.
  [self provideCredentialForIdentity:credentialIdentity];
}

- (void)prepareInterfaceToProvideCredentialForIdentity:
    (ASPasswordCredentialIdentity*)credentialIdentity {
  [self reauthenticateIfNeededWithCompletionHandler:^(
            ReauthenticationResult result) {
    if (result != ReauthenticationResult::kFailure) {
      [self provideCredentialForIdentity:credentialIdentity];
    } else {
      NSError* error =
          [[NSError alloc] initWithDomain:ASExtensionErrorDomain
                                     code:ASExtensionErrorCodeUserCanceled
                                 userInfo:nil];
      [self.extensionContext cancelRequestWithError:error];
    }
  }];
}

- (void)prepareInterfaceForExtensionConfiguration {
  // Reset the consent if the extension was disabled and reenabled.
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  [shared_defaults
      removeObjectForKey:kUserDefaultsCredentialProviderConsentVerified];
  self.consentCoordinator = [[ConsentCoordinator alloc]
         initWithBaseViewController:self
                            context:self.extensionContext
            reauthenticationHandler:self.reauthenticationHandler
      isInitialConfigurationRequest:YES];
  [self.consentCoordinator start];
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

// Completes the extension request providing |ASPasswordCredential| that matches
// the |credentialIdentity| or an error if not found.
- (void)provideCredentialForIdentity:
    (ASPasswordCredentialIdentity*)credentialIdentity {
  NSString* identifier = credentialIdentity.recordIdentifier;
  id<Credential> credential =
      [self.credentialStore credentialWithIdentifier:identifier];
  if (credential) {
    NSString* password =
        PasswordWithKeychainIdentifier(credential.keychainIdentifier);
    if (password) {
      ASPasswordCredential* ASCredential =
          [ASPasswordCredential credentialWithUser:credential.user
                                          password:password];
      [self.extensionContext completeRequestWithSelectedCredential:ASCredential
                                                 completionHandler:nil];
      return;
    }
  }
  NSError* error = [[NSError alloc] initWithDomain:ASExtensionErrorDomain
                                              code:ASExtensionErrorCodeFailed
                                          userInfo:nil];
  [self.extensionContext cancelRequestWithError:error];
}

#pragma mark - SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  self.lastSuccessfulReauthTime = [[NSDate alloc] init];
}

@end
