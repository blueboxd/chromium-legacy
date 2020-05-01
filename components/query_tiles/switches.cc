// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/switches.h"

namespace upboarding {
namespace features {
const base::Feature kQueryTiles{"QueryTiles",
                                base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kQueryTilesInOmnibox{"QueryTilesInOmnibox",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace features

namespace switches {
const char kQueryTilesCountryCode[] = "query-tiles-country-code";

const char kQueryTilesInstantBackgroundTask[] =
    "query-tiles-instant-background-task";
}  // namespace switches
}  // namespace upboarding
