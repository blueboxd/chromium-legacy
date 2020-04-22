// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_util.h"

#include "base/metrics/histogram_macros.h"
#include "components/google/core/common/google_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

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
