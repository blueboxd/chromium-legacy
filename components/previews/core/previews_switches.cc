// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_switches.h"

#include "base/command_line.h"

namespace previews {
namespace switches {

bool ShouldIgnorePreviewsBlacklist() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             kIgnorePreviewsBlacklist) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             kForceEnablePreviews);
}

// Do not require the user notification InfoBar to be shown before triggering a
// Lite Page Redirect preview.
const char kDoNotRequireLitePageRedirectInfoBar[] =
    "dont-require-litepage-redirect-infobar";

// Ignore decisions made by PreviewsBlackList.
const char kIgnorePreviewsBlacklist[] = "ignore-previews-blacklist";

// Force enable all available previews on every page load.
const char kForceEnablePreviews[] = "force-enable-lite-pages";

// Override the Lite Page Preview Host.
const char kLitePageServerPreviewHost[] = "litepage-server-previews-host";

// Ignore the optimization hints blacklist for Lite Page Redirect previews.
const char kIgnoreLitePageRedirectOptimizationBlacklist[] =
    "ignore-litepage-redirect-optimization-blacklist";

// Clears the local Lite Page Redirect blacklist on startup.
const char kClearLitePageRedirectLocalBlacklist[] =
    "clear-litepage-redirect-local-blacklist-on-startup";

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

// Sets the trigger ordering of Lite Page Redirect to be higher than page hints.
const char kLitePageRedirectOverridesPageHints[] =
    "litepage_redirect_overrides_page_hints";

}  // namespace switches
}  // namespace previews
