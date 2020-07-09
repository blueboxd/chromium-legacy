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
const char prefs::kNearbySharingDeviceIdPrefName[] = "nearby_sharing.device_id";
const char prefs::kNearbySharingDeviceNamePrefName[] =
    "nearby_sharing.device_name";
const char prefs::kNearbySharingFullNamePrefName[] = "nearby_sharing.full_name";
const char prefs::kNearbySharingIconUrlPrefName[] = "nearby_sharing.icon_url";

void RegisterNearbySharingPrefs(user_prefs::PrefRegistrySyncable* registry) {
  // These prefs are not synced across devices on purpose.

  // TODO(vecore): Change the default to false after the settings ui is
  // available.
  registry->RegisterBooleanPref(prefs::kNearbySharingEnabledPrefName,
                                /*default_value=*/true);
  registry->RegisterIntegerPref(
      prefs::kNearbySharingBackgroundVisibilityName,
      /*default_value=*/static_cast<int>(Visibility::kNoOne));
  registry->RegisterIntegerPref(
      prefs::kNearbySharingDataUsageName,
      /*default_value=*/static_cast<int>(DataUsage::kWifiOnly));
  registry->RegisterStringPref(prefs::kNearbySharingDeviceIdPrefName,
                               /*default_value=*/std::string());
  registry->RegisterStringPref(prefs::kNearbySharingDeviceNamePrefName,
                               /*default_value=*/std::string());
  registry->RegisterStringPref(prefs::kNearbySharingFullNamePrefName,
                               /*default_value=*/std::string());
  registry->RegisterStringPref(prefs::kNearbySharingIconUrlPrefName,
                               /*default_value=*/std::string());
}

void RegisterNearbySharingLocalPrefs(PrefRegistrySimple* local_state) {
  local_state->RegisterFilePathPref(prefs::kNearbySharingActiveProfilePrefName,
                                    /*default_value=*/base::FilePath());
}
