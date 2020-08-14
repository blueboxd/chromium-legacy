// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/well_known_change_password_util.h"

#include "base/logging.h"
#include "url/gurl.h"

namespace password_manager {

const char kWellKnownChangePasswordPath[] = "/.well-known/change-password";

const char kWellKnownNotExistingResourcePath[] =
    "/.well-known/"
    "resource-that-should-not-exist-whose-status-code-should-not-be-200";

// .well-known/change-password is a defined standard that points to the sites
// change password form. https://wicg.github.io/change-password-url/
bool IsWellKnownChangePasswordUrl(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || !url.has_path())
    return false;
  base::StringPiece path = url.PathForRequestPiece();
  // remove trailing slash if there
  if (path.ends_with("/"))
    path = path.substr(0, path.size() - 1);
  return path == kWellKnownChangePasswordPath;
}

GURL CreateWellKnownNonExistingResourceURL(const GURL& url) {
  GURL::Replacements replacement;
  replacement.SetPathStr(kWellKnownNotExistingResourcePath);
  return url.ReplaceComponents(replacement);
}

}  // namespace password_manager
