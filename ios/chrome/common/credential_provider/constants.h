// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CONSTANTS_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Path to the persisted file for the credential provider archivable store.
NSURL* CredentialProviderSharedArchivableStoreURL();

// Key for the app group user defaults indicating if the credentials have been
// synced with iOS via AuthenticationServices.
extern NSString* const
    kUserDefaultsCredentialProviderASIdentityStoreSyncCompleted;

// Key for the app group user defaults indicating if the credentials have been
// sync for the first time. The defaults contain a Bool indicating if the first
// time sync have been completed.
extern NSString* const kUserDefaultsCredentialProviderFirstTimeSyncCompleted;

// Key for the app group user defaults indicating if the user has enabled and
// given consent for the credential provider extension.
extern NSString* const kUserDefaultsCredentialProviderConsentVerified;

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CONSTANTS_H_
