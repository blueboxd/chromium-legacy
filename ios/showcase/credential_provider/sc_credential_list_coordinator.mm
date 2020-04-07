// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/credential_provider/sc_credential_list_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/credential_provider_extension/ui/credential_details_consumer.h"
#import "ios/chrome/credential_provider_extension/ui/credential_details_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_consumer.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCCredential : NSObject <Credential>
@end

@implementation SCCredential
@synthesize favicon = _favicon;
@synthesize keychainIdentifier = _keychainIdentifier;
@synthesize rank = _rank;
@synthesize recordIdentifier = _recordIdentifier;
@synthesize serviceIdentifier = _serviceIdentifier;
@synthesize serviceName = _serviceName;
@synthesize user = _user;
@synthesize validationIdentifier = _validationIdentifier;

- (instancetype)initWithServiceName:(NSString*)serviceName
                               user:(NSString*)user {
  self = [super init];
  if (self) {
    _serviceName = serviceName;
    _user = user;
  }
  return self;
}

@end

namespace {
NSArray<id<Credential>>* suggestedPasswords = @[
  [[SCCredential alloc] initWithServiceName:@"www.domain.com"
                                       user:@"johnsmith"],
  [[SCCredential alloc] initWithServiceName:@"www.domain.com"
                                       user:@"janesmythe"],
];
NSArray<id<Credential>>* allPasswords = @[
  [[SCCredential alloc] initWithServiceName:@"www.domain1.com"
                                       user:@"jsmythe@fazebook.com"],
  [[SCCredential alloc] initWithServiceName:@"www.domain2.com"
                                       user:@"jasmith@twitcher.com"],
  [[SCCredential alloc] initWithServiceName:@"www.domain3.com"
                                       user:@"HughZername"],
];
}

@interface SCCredentialListCoordinator () <CredentialDetailsConsumerDelegate,
                                           CredentialListConsumerDelegate>
@property(nonatomic, strong) CredentialListViewController* viewController;
@property(nonatomic, strong)
    CredentialDetailsViewController* detailsViewController;
@end

@implementation SCCredentialListCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize viewController = _viewController;

- (void)start {
  self.viewController = [[CredentialListViewController alloc] init];
  self.viewController.title = @"CPE Passwords";
  self.viewController.delegate = self;
  [self.baseViewController setHidesBarsOnSwipe:NO];
  [self.baseViewController pushViewController:self.viewController animated:YES];

  self.detailsViewController = [[CredentialDetailsViewController alloc] init];
  self.detailsViewController.title = @"CPE Password Details";
  self.detailsViewController.delegate = self;

  [self.viewController presentSuggestedPasswords:suggestedPasswords
                                    allPasswords:allPasswords];
}

#pragma mark - CredentialListConsumerDelegate

- (void)navigationCancelButtonWasPressed:(UIButton*)button {
}

- (void)updateResultsWithFilter:(NSString*)filter {
  // TODO(crbug.com/1045454): Implement this method.
  NSMutableArray<id<Credential>>* suggested = [[NSMutableArray alloc] init];
  for (id<Credential> credential in suggestedPasswords) {
    if ([filter length] == 0 ||
        [credential.serviceName localizedStandardContainsString:filter] ||
        [credential.user localizedStandardContainsString:filter]) {
      [suggested addObject:credential];
    }
  }
  NSMutableArray<id<Credential>>* all = [[NSMutableArray alloc] init];
  for (id<Credential> credential in allPasswords) {
    if ([filter length] == 0 ||
        [credential.serviceName localizedStandardContainsString:filter] ||
        [credential.user localizedStandardContainsString:filter]) {
      [all addObject:credential];
    }
  }
  [self.viewController presentSuggestedPasswords:suggested allPasswords:all];
}

#pragma mark - CredentialDetailsConsumerDelegate

- (void)unlockPasswordForCredential:(id<Credential>)credential
                  completionHandler:(void (^)(NSString*))completionHandler {
  completionHandler(@"DreamOn");
}

- (void)showDetailsForCredential:(id<Credential>)credential {
  [self.detailsViewController presentCredential:credential];
  [self.baseViewController pushViewController:self.detailsViewController
                                     animated:YES];
}

@end
