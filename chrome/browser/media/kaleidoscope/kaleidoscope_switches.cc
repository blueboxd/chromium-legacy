// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_switches.h"

#include "base/command_line.h"
#include "url/gurl.h"

namespace switches {

// A command line switch for overriding the base URL of the API.
const char kKaleidoscopeBackendUrl[] = "kaleidoscope-backend-url";

// The command line alias and URL for the "prod" environment.
const char kKaleidoscopeBackendUrlProdAlias[] = "prod";
const char kKaleidoscopeBackendUrlProdUrl[] =
    "https://chromemediarecommendations-pa.googleapis.com";

// The command line alias and URL for the "staging" environment.
const char kKaleidoscopeBackendUrlStagingAlias[] = "staging";
const char kKaleidoscopeBackendUrlStagingUrl[] =
    "https://staging-chromemediarecommendations-pa.sandbox.googleapis.com";

// The command line alias and URL for the "autopush" environment.
const char kKaleidoscopeBackendUrlAutopushAlias[] = "autopush";
const char kKaleidoscopeBackendUrlAutopushUrl[] =
    "https://autopush-chromemediarecommendations-pa.sandbox.googleapis.com";

}  // namespace switches

GURL GetGoogleAPIBaseURL(const base::CommandLine& command_line) {
  // Return the URL set in the command line, if any.
  if (command_line.HasSwitch(switches::kKaleidoscopeBackendUrl)) {
    auto value =
        command_line.GetSwitchValueASCII(switches::kKaleidoscopeBackendUrl);

    // If the value is a valid base URL then return it.
    GURL url(value);
    if (url.is_valid() && (url.path().empty() || url.path() == "/")) {
      return url;
    }

    // Check if the value is an alias and return it.
    if (value == switches::kKaleidoscopeBackendUrlProdAlias) {
      return GURL(switches::kKaleidoscopeBackendUrlProdUrl);
    } else if (value == switches::kKaleidoscopeBackendUrlStagingAlias) {
      return GURL(switches::kKaleidoscopeBackendUrlStagingUrl);
    } else if (value == switches::kKaleidoscopeBackendUrlAutopushAlias) {
      return GURL(switches::kKaleidoscopeBackendUrlAutopushUrl);
    }
  }

  return GURL(switches::kKaleidoscopeBackendUrlProdUrl);
}
