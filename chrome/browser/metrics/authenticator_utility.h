// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_AUTHENTICATOR_UTILITY_H_
#define CHROME_BROWSER_METRICS_AUTHENTICATOR_UTILITY_H_

namespace authenticator_utility {

// Report whether the current system has a user-verifying platform
// authenticator available on desktop platforms.
void ReportUVPlatformAuthenticatorAvailability();

}  // namespace authenticator_utility

#endif  // CHROME_BROWSER_METRICS_AUTHENTICATOR_UTILITY_H_
