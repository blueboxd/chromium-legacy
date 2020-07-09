// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"

namespace safe_browsing {

void SafeBrowsingUrlCheckerImpl::LogRTLookupRequest(
    const RTLookupRequest& request,
    const std::string& oauth_token) {
  // TODO(crbug.com/1103222): Log this request on open chrome://safe-browsing
  // pages once chrome://safe-browsing works on iOS, or log this request to
  // stderr.
}

void SafeBrowsingUrlCheckerImpl::LogRTLookupResponse(
    const RTLookupResponse& response) {
  // TODO(crbug.com/1103222): Log this response on open chrome://safe-browsing
  // pages once chrome://safe-browsing works on iOS, or log this response to
  // stderr.
}

}  // namespace safe_browsing
