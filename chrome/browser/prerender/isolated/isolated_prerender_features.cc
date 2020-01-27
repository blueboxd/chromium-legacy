// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"

namespace features {

// Forces all eligible prerenders to be done in an isolated manner such that no
// user-identifying information is used during the prefetch.
const base::Feature kIsolatePrerenders{"IsolatePrerenders",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Forces all isolated prerenders to be proxied through a CONNECT tunnel.
const base::Feature kIsolatedPrerenderUsesProxy{
    "IsolatedPrerenderUsesProxy", base::FEATURE_DISABLED_BY_DEFAULT};

// Prefetches main frame HTML resources for results on Google SRPs.
const base::Feature kPrefetchSRPNavigationPredictions_HTMLOnly{
    "PrefetchSRPNavigationPredictions_HTMLOnly",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
