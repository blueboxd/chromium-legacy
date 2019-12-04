// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DM_TOKEN_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_DM_TOKEN_UTILS_H_

#include "components/policy/core/common/cloud/dm_token.h"

class Profile;

namespace safe_browsing {

// Helper function to get DM token to use with uploads.
// On Browser platforms gets machine-level DM token,
// On Chrome OS gets user DM token for the profile if it's associated with an
// affiliated user.
policy::DMToken GetDMToken(Profile* const profile);

// Overrides the DM token used for testing purposes.
void SetDMTokenForTesting(const policy::DMToken& dm_token);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DM_TOKEN_UTILS_H_
