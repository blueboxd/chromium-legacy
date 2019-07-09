// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_switches.h"

#include "base/command_line.h"

namespace optimization_guide {
namespace switches {

// Overrides the Hints Protobuf that would come from the component updater. If
// the value of this switch is invalid, regular hint processing is used.
// The value of this switch should be a base64 encoding of a binary
// Configuration message, found in optimization_guide's hints.proto. Providing a
// valid value to this switch causes Chrome startup to block on hints parsing.
const char kHintsProtoOverride[] = "optimization_guide_hints_override";

// Overrides scheduling and time delays for fetching hints and causes a hints
// fetch immediately on start up using the provided comma separate lists of
// hosts.
const char kFetchHintsOverride[] = "optimization-guide-fetch-hints-override";

// Overrides the hints fetch scheduling and delay, causing a hints fetch
// immediately on start up using the TopHostProvider. This is meant for testing.
const char kFetchHintsOverrideTimer[] =
    "optimization-guide-fetch-hints-override-timer";

// Overrides the Optimization Guide Service URL that the HintsFetcher will
// request remote hints from.
const char kOptimizationGuideServiceURL[] = "optimization-guide-service-url";

// Overrides the Optimization Guide Service API Key for remote requests to be
// made.
const char kOptimizationGuideServiceAPIKey[] =
    "optimization-guide-service-api-key";

// Purges the hint cache store on startup, so that it's guaranteed to be using
// fresh data.
const char kPurgeHintCacheStore[] = "purge_hint_cache_store";

}  // namespace switches
}  // namespace optimization_guide
