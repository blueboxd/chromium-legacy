// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/external_app_install_features.h"

#include "base/feature_list.h"

namespace web_app {

namespace {

// A hard coded list of features available for externally installed apps to gate
// their installation on via their config file settings.
constexpr base::Feature kExternalAppInstallFeatures[] = {};

bool g_always_enabled_for_testing = false;

}  // namespace

bool IsExternalAppInstallFeatureEnabled(base::StringPiece feature_name) {
  if (g_always_enabled_for_testing)
    return true;

  for (const base::Feature& feature : kExternalAppInstallFeatures) {
    if (feature.name == feature_name)
      return base::FeatureList::IsEnabled(feature);
  }

  return false;
}

base::AutoReset<bool> SetExternalAppInstallFeatureAlwaysEnabledForTesting() {
  return base::AutoReset<bool>(&g_always_enabled_for_testing, true);
}

}  // namespace web_app
