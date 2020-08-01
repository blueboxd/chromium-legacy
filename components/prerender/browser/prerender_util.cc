// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prerender/browser/prerender_util.h"

#include "base/metrics/histogram_macros.h"
#include "components/google/core/common/google_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace prerender {

bool IsGoogleOriginURL(const GURL& origin_url) {
  // ALLOW_NON_STANDARD_PORTS for integration tests with the embedded server.
  if (!google_util::IsGoogleDomainUrl(origin_url,
                                      google_util::DISALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS)) {
    return false;
  }

  return (origin_url.path_piece() == "/") ||
         google_util::IsGoogleSearchUrl(origin_url);
}

}  // namespace prerender
