// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SWITCHES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SWITCHES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"

namespace optimization_guide {
namespace proto {
class Configuration;
}

namespace switches {

extern const char kHintsProtoOverride[];
extern const char kFetchHintsOverride[];
extern const char kFetchHintsOverrideTimer[];
extern const char kOptimizationGuideServiceURL[];
extern const char kOptimizationGuideServiceAPIKey[];
extern const char kPurgeHintCacheStore[];

// Returns whether the hint component should be processed.
// Available hint components are only processed if a proto override isn't being
// used; otherwise, the hints from the proto override are used instead.
bool IsHintComponentProcessingDisabled();

// Returns whether hints should be purged during startup if the explicit purge
// switch exists or if a proto override is being used--in which case the hints
// need to come from the override instead.
bool ShouldPurgeHintCacheStoreOnStartup();

// Parses a list of hosts to have hints fetched for. This overrides scheduling
// of the first hints fetch and forces it to occur immediately. If no hosts are
// provided, nullopt is returned.
base::Optional<std::vector<std::string>>
ParseHintsFetchOverrideFromCommandLine();

// Whether the hints fetcher timer should be overridden.
bool ShouldOverrideFetchHintsTimer();

// Attempts to parse a base64 encoded Optimization Guide Configuration proto
// from the command line. If no proto is given or if it is encoded incorrectly,
// nullptr is returned.
std::unique_ptr<optimization_guide::proto::Configuration>
ParseComponentConfigFromCommandLine();

}  // namespace switches
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SWITCHES_H_
