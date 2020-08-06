// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"

#include "chrome/browser/reputation/url_elision_policy.h"
#include "components/omnibox/common/omnibox_features.h"

#include "url/gurl.h"

namespace {

const base::FeatureParam<int> kMaximumUnelidedHostnameLength{
    &omnibox::kMaybeElideToRegistrableDomain, "max_unelided_host_length", 25};

}  // namespace

bool ShouldElideToRegistrableDomain(const GURL& url) {
  DCHECK(base::FeatureList::IsEnabled(omnibox::kMaybeElideToRegistrableDomain));
  if (url.HostIsIPAddress()) {
    return false;
  }

  // TODO(jdeblasio): Check allowlist

  if (static_cast<int>(url.host().length()) >
      kMaximumUnelidedHostnameLength.Get()) {
    return true;
  }

  // TODO(jdeblasio): Check for keywords

  return false;
}
