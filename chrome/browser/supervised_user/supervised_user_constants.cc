// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_constants.h"

namespace supervised_users {

#if defined(OS_CHROMEOS)
const char kAccountConsistencyMirrorRequired[] =
    "AccountConsistencyMirrorRequired";
#endif
const char kApprovedExtensions[] = "ApprovedExtensions";
const char kAuthorizationHeaderFormat[] = "Bearer %s";
const char kCameraMicDisabled[] = "CameraMicDisabled";
const char kContentPackDefaultFilteringBehavior[] =
    "ContentPackDefaultFilteringBehavior";
const char kContentPackManualBehaviorHosts[] = "ContentPackManualBehaviorHosts";
const char kContentPackManualBehaviorURLs[] = "ContentPackManualBehaviorURLs";
const char kCookiesAlwaysAllowed[] = "CookiesAlwaysAllowed";
const char kForceSafeSearch[] = "ForceSafeSearch";
const char kGeolocationDisabled[] = "GeolocationDisabled";
const char kRecordHistory[] = "RecordHistory";
const char kSafeSitesEnabled[] = "SafeSites";
const char kSigninAllowed[] = "SigninAllowed";
const char kUserName[] = "UserName";

// NOTE: Do not change this value without changing the value of the
// corresponding constant in
// //components/signin/public/identity_manager/identity_manager.cc to
// correspond.
const char kSupervisedUserPseudoEmail[] = "managed_user@localhost";

const char kChildAccountSUID[] = "ChildAccountSUID";

const char kChromeAvatarIndex[] = "chrome-avatar-index";
const char kChromeOSAvatarIndex[] = "chromeos-avatar-index";

const char kChromeOSPasswordData[] = "chromeos-password-data";

}  // namespace supervised_users
