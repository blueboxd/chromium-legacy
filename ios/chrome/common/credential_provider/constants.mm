// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/constants.h"

#include "ios/chrome/common/ios_app_bundle_id_prefix_buildflags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Filename for the archivable storage.
NSString* const kArchivableStorageFilename = @"credential_store";

// Credential Provider dedicated shared folder name.
NSString* const kCredentialProviderContainer = @"credential_provider";

// Returns the app group for the current build.
NSString* ApplicationGroup() {
  return [NSString stringWithFormat:@"group.%s.chrome",
                                    BUILDFLAG(IOS_APP_BUNDLE_ID_PREFIX), nil];
}

}  // namespace

NSURL* CredentialProviderSharedArchivableStoreURL() {
  NSURL* groupURL = [[NSFileManager defaultManager]
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];
  NSURL* credentialProviderURL =
      [groupURL URLByAppendingPathComponent:kCredentialProviderContainer];
  return [credentialProviderURL
      URLByAppendingPathComponent:kArchivableStorageFilename];
}
