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

// Allows defer script preview on all https pages even if optimization hints are
// missing for that webpage.
const char kEnableDeferAllScriptWithoutOptimizationHints[] =
    "enable-defer-all-script-without-optimization-hints";

}  // namespace switches
}  // namespace previews
