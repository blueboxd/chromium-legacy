// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/pending_network_configuration_tracker_impl.h"

#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

const char kPendingNetworkConfigurationsPref[] =
    "sync_wifi.pending_network_configuration_updates";
const char kChangeGuidKey[] = "ChangeGuid";
const char kSpecificsKey[] = "Specifics";

std::string GeneratePath(const std::string& ssid, const std::string& subkey) {
  return base::StringPrintf("%s.%s", ssid.c_str(), subkey.c_str());
}

}  // namespace

namespace sync_wifi {

// static
void PendingNetworkConfigurationTrackerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kPendingNetworkConfigurationsPref);
}

PendingNetworkConfigurationTrackerImpl::PendingNetworkConfigurationTrackerImpl(
    PrefService* pref_service)
    : pref_service_(pref_service),
      dict_(pref_service_->GetDictionary(kPendingNetworkConfigurationsPref)
                ->Clone()) {}

PendingNetworkConfigurationTrackerImpl::
    ~PendingNetworkConfigurationTrackerImpl() = default;

void PendingNetworkConfigurationTrackerImpl::TrackPendingUpdate(
    const std::string& change_guid,
    const std::string& ssid,
    const base::Optional<sync_pb::WifiConfigurationSpecificsData>& specifics) {
  std::string serialized_specifics;
  if (!specifics)
    serialized_specifics = std::string();
  else
    CHECK(specifics->SerializeToString(&serialized_specifics));

  dict_.SetPath(GeneratePath(ssid, kChangeGuidKey), base::Value(change_guid));
  dict_.SetPath(GeneratePath(ssid, kSpecificsKey),
                base::Value(serialized_specifics));
  pref_service_->Set(kPendingNetworkConfigurationsPref, dict_);
}

void PendingNetworkConfigurationTrackerImpl::MarkComplete(
    const std::string& change_guid,
    const std::string& ssid) {
  if (!IsChangeTracked(change_guid, ssid))
    return;

  dict_.RemovePath(ssid);
  pref_service_->Set(kPendingNetworkConfigurationsPref, dict_);
}

bool PendingNetworkConfigurationTrackerImpl::IsChangeTracked(
    const std::string& change_guid,
    const std::string& ssid) {
  std::string* found_id =
      dict_.FindStringPath(GeneratePath(ssid, kChangeGuidKey));
  return found_id && *found_id == change_guid;
}

std::vector<PendingNetworkConfigurationUpdate>
PendingNetworkConfigurationTrackerImpl::GetPendingUpdates() {
  std::vector<PendingNetworkConfigurationUpdate> list;
  for (const auto& entry : dict_.DictItems()) {
    base::Optional<sync_pb::WifiConfigurationSpecificsData> specifics;
    std::string* specifics_string = entry.second.FindStringKey(kSpecificsKey);
    if (!specifics_string->empty()) {
      sync_pb::WifiConfigurationSpecificsData data;
      data.ParseFromString(*specifics_string);
      specifics = data;
    }
    std::string* change_guid = entry.second.FindStringKey(kChangeGuidKey);
    list.emplace_back(/*ssid=*/entry.first, *change_guid, specifics);
  }
  return list;
}

}  // namespace sync_wifi
