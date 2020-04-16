// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"

namespace safe_browsing {

bool SafeBrowsingUrlCheckerImpl::CanPerformFullURLLookup(const GURL& url) {
  // TODO(crbug.com/1028755): Support real-time checks on iOS.
  return false;
}

void SafeBrowsingUrlCheckerImpl::OnRTLookupRequest(
    std::unique_ptr<RTLookupRequest> request,
    std::string oauth_token) {
  NOTREACHED();
}

void SafeBrowsingUrlCheckerImpl::OnRTLookupResponse(
    bool is_rt_lookup_successful,
    std::unique_ptr<RTLookupResponse> response) {
  NOTREACHED();
}

}  // namespace safe_browsing
