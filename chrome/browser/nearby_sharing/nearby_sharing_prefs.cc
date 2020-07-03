// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_prefs.h"

#include <string>

#include "base/files/file_path.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"

const char prefs::kNearbySharingEnabledPrefName[] = "nearby_sharing.enabled";
const char prefs::kNearbySharingActiveProfilePrefName[] =
    "nearby_sharing.active_profile";
const char prefs::kNearbySharingBackgroundVisibilityName[] =
    "nearby_sharing.background_visibility";
const char prefs::kNearbySharingDataUsageName[] = "nearby_sharing.data_usage";

void RegisterNearbySharingPrefs(user_prefs::PrefRegistrySyncable* registry) {
  // This pref is not synced.
  // TODO(vecore): Change the default to false after the settings ui is
  // available.
  registry->RegisterBooleanPref(
      prefs::kNearbySharingEnabledPrefName, true /* default_value */,
      PrefRegistry::PrefRegistrationFlags::NO_REGISTRATION_FLAGS /* flags */);
  registry->RegisterIntegerPref(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(Visibility::kNoOne) /* default_value */,
      PrefRegistry::PrefRegistrationFlags::NO_REGISTRATION_FLAGS /* flags */);
  registry->RegisterIntegerPref(
      prefs::kNearbySharingDataUsageName,
      static_cast<int>(DataUsage::kWifiOnly) /* default_value */,
      PrefRegistry::PrefRegistrationFlags::NO_REGISTRATION_FLAGS /* flags */);
}

void RegisterNearbySharingLocalPrefs(PrefRegistrySimple* local_state) {
  local_state->RegisterFilePathPref(prefs::kNearbySharingActiveProfilePrefName,
                                    base::FilePath() /* default_value */);
}
