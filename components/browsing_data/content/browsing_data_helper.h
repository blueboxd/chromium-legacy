// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines methods relevant to all code that wants to work with browsing data.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_HELPER_H_

#include <string>

#include "base/macros.h"

class GURL;

namespace browsing_data {

// TODO(crbug.com/668114): DEPRECATED. Remove these functions.
// The primary functionality of testing origin type masks has moved to
// Remover. The secondary functionality of recognizing web schemes
// storing browsing data has moved to url::GetWebStorageSchemes();
// or alternatively, it can also be retrieved from Remover by
// testing the ORIGIN_TYPE_UNPROTECTED_ORIGIN | ORIGIN_TYPE_PROTECTED_ORIGIN
// origin mask. This class now merely forwards the functionality to several
// helper classes in the browsing_data codebase.

// Returns true iff the provided scheme is (really) web safe, and suitable
// for treatment as "browsing data". This relies on the definition of web safe
// in ChildProcessSecurityPolicy, but excluding schemes like
// `chrome-extension`.
bool IsWebScheme(const std::string& scheme);
bool HasWebScheme(const GURL& origin);

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_HELPER_H_
