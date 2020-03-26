// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "ios/chrome/credential_provider_extension/ui/credential_list_consumer.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_ui_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialListMediator () <CredentialListConsumerDelegate>

// The UI Handler of the feature.
@property(nonatomic, weak) id<CredentialListUIHandler> UIHandler;

// The consumer for this mediator.
@property(nonatomic, weak) id<CredentialListConsumer> consumer;

// The service identifiers to be prioritized.
@property(nonatomic, strong)
    NSArray<ASCredentialServiceIdentifier*>* serviceIdentifiers;

// The extension context in which the credential list was started.
@property(nonatomic, weak) ASCredentialProviderExtensionContext* context;

@end

@implementation CredentialListMediator

- (instancetype)initWithConsumer:(id<CredentialListConsumer>)consumer
                       UIHandler:(id<CredentialListUIHandler>)UIHandler
                         context:(ASCredentialProviderExtensionContext*)context
              serviceIdentifiers:
                  (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers {
  self = [super init];
  if (self) {
    _serviceIdentifiers = serviceIdentifiers ?: @[];
    _UIHandler = UIHandler;
    _consumer = consumer;
    _consumer.delegate = self;
    _context = context;
  }
  return self;
}

- (void)fetchCredentials {
  // TODO(crbug.com/1045454): Implement this method. For now present the empty
  // credentials screen all the time.
  [self.UIHandler showEmptyCredentials];
}

#pragma mark - CredentialListConsumerDelegate

- (void)navigationCancelButtonWasPressed:(UIButton*)button {
  NSError* error =
      [[NSError alloc] initWithDomain:ASExtensionErrorDomain
                                 code:ASExtensionErrorCodeUserCanceled
                             userInfo:nil];
  [self.context cancelRequestWithError:error];
}

- (void)updateResultsWithFilter:(NSString*)filter {
  // TODO(crbug.com/1045454): Implement this method.
}

@end
