// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/common/credential_provider/credential.h"

@interface ArchivableCredential : NSObject <Credential, NSSecureCoding>

- (instancetype)initWithFavicon:(NSString*)favicon
             keychainIdentifier:(NSString*)keychainIdentifier
                           rank:(int64_t)rank
               recordIdentifier:(NSString*)recordIdentifier
              serviceIdentifier:(NSString*)serviceIdentifier
                    serviceName:(NSString*)serviceName
                           user:(NSString*)user
           validationIdentifier:(NSString*)validationIdentifier
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_H_
