// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_SWITCHES_H_
#define COMPONENTS_QUERY_TILES_SWITCHES_H_

#include "base/feature_list.h"

namespace upboarding {
namespace features {

// Main feature flag for the query tiles feature.
extern const base::Feature kQueryTiles;

// Feature flag to determine whether query tiles should be shown on omnibox.
extern const base::Feature kQueryTilesInOmnibox;

}  // namespace features

namespace switches {

// If set, this value overrides the default country code to be sent to the
// server when fetching tiles.
extern const char kQueryTilesCountryCode[];

// If set, the background task will be started after a short period.
extern const char kQueryTilesInstantBackgroundTask[];

}  // namespace switches
}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_SWITCHES_H_
