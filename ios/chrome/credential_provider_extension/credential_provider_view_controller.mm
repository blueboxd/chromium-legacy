// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/credential_provider_view_controller.h"

#import "ios/chrome/credential_provider_extension/ui/credential_list_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialProviderViewController ()

// List coordinator that shows the list of passwords when started.
@property(nonatomic, strong) CredentialListCoordinator* listCoordinator;

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

@end
