// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_prefs.h"

#include <algorithm>
#include <vector>

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace syncer {
namespace {

// The GUID device info will use to to remember the most recently used past
// cache GUIDs in order starting with the current cache GUID.
const char kDeviceInfoRecentGUIDs[] = "sync.local_device_guids";

// The max number of local device most recent cached GUIDSs that will be stored.
const int kMaxLocalCacheGuidsStored = 30;

}  // namespace

// static
void DeviceInfoPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kDeviceInfoRecentGUIDs);
}

DeviceInfoPrefs::DeviceInfoPrefs(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service);
}

DeviceInfoPrefs::~DeviceInfoPrefs() {}

bool DeviceInfoPrefs::IsRecentLocalCacheGuid(
    const std::string& cache_guid) const {
  const base::Value::ListStorage& recent_local_cache_guids =
      pref_service_->GetList(kDeviceInfoRecentGUIDs)->GetList();

  return std::find(recent_local_cache_guids.begin(),
                   recent_local_cache_guids.end(),
                   base::Value(cache_guid)) != recent_local_cache_guids.end();
}

void DeviceInfoPrefs::AddLocalCacheGuid(const std::string& cache_guid) {
  ListPrefUpdate update(pref_service_, kDeviceInfoRecentGUIDs);
  base::ListValue* pref_data = update.Get();
  base::Value::ListStorage* recent_local_cache_guids = &pref_data->GetList();

  if (std::find(recent_local_cache_guids->begin(),
                recent_local_cache_guids->end(),
                base::Value(cache_guid)) != recent_local_cache_guids->end()) {
    // Local cache GUID already known.
    return;
  }

  recent_local_cache_guids->emplace(recent_local_cache_guids->begin(),
                                    base::Value(cache_guid));
  if (recent_local_cache_guids->size() > kMaxLocalCacheGuidsStored) {
    recent_local_cache_guids->pop_back();
  }
}

}  // namespace syncer
