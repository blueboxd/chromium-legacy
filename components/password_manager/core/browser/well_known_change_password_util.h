// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WELL_KNOWN_CHANGE_PASSWORD_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WELL_KNOWN_CHANGE_PASSWORD_UTIL_H_

#include <memory>

class GURL;

namespace password_manager {

// Path for Well-Known change password url
// Spec: https://wicg.github.io/change-password-url/
extern const char kWellKnownChangePasswordPath[];

// This path should return 404. This enables us to check whether
// we can trust the server's Well-Known response codes.
// https://wicg.github.io/change-password-url/response-code-reliability.html#iana
extern const char kWellKnownNotExistingResourcePath[];

// .well-known/change-password is a defined standard that points to the sites
// change password form.
// https://wicg.github.io/change-password-url/
bool IsWellKnownChangePasswordUrl(const GURL& url);

// Creates a GURL for a given origin with |kWellKnownNotExistingResourcePath| as
// path.
GURL CreateWellKnownNonExistingResourceURL(const GURL& url);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WELL_KNOWN_CHANGE_PASSWORD_UTIL_H_