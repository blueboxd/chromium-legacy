// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "components/signin/public/identity_manager/account_info.h"

@implementation FakeChromeIdentity {
  NSString* _userEmail;
  NSString* _gaiaID;
  NSString* _userFullName;
  NSString* _hashedGaiaID;
  NSString* _hostedDomain;
}

+ (FakeChromeIdentity*)identityWithEmail:(NSString*)email
                                  gaiaID:(NSString*)gaiaID
                                    name:(NSString*)name {
  return [[FakeChromeIdentity alloc] initWithEmail:email
                                            gaiaID:gaiaID
                                              name:name
                                      hostedDomain:@(kNoHostedDomainFound)];
}

+ (FakeChromeIdentity*)identityWithEmail:(NSString*)email
                                  gaiaID:(NSString*)gaiaID
                                    name:(NSString*)name
                            hostedDomain:(NSString*)hostedDomain {
  return [[FakeChromeIdentity alloc] initWithEmail:email
                                            gaiaID:gaiaID
                                              name:name
                                      hostedDomain:hostedDomain];
}

- (instancetype)initWithEmail:(NSString*)email
                       gaiaID:(NSString*)gaiaID
                         name:(NSString*)name
                 hostedDomain:(NSString*)hostedDomain {
  self = [super init];
  if (self) {
    _userEmail = [email copy];
    _gaiaID = [gaiaID copy];
    _userFullName = [name copy];
    _hashedGaiaID = [NSString stringWithFormat:@"%@_hashID", name];
    _hostedDomain = hostedDomain;
  }
  return self;
}

- (NSString*)userEmail {
  return _userEmail;
}

- (NSString*)gaiaID {
  return _gaiaID;
}

- (NSString*)userFullName {
  return _userFullName;
}

- (NSString*)hashedGaiaID {
  return _hashedGaiaID;
}

- (NSString*)hostedDomain {
  return _hostedDomain;
}

@end
