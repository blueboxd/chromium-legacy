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
const char kCompletedAttemptsKey[] = "CompletedAttempts";
const char kSpecificsKey[] = "Specifics";

std::string GeneratePath(const std::string& ssid, const std::string& subkey) {
  return base::StringPrintf("%s.%s", ssid.c_str(), subkey.c_str());
}

sync_wifi::PendingNetworkConfigurationUpdate ConvertToPendingUpdate(
    base::Value* dict,
    const std::string ssid) {
  std::string* change_guid = dict->FindStringKey(kChangeGuidKey);
  base::Optional<sync_pb::WifiConfigurationSpecificsData> specifics;
  std::string* specifics_string = dict->FindStringKey(kSpecificsKey);
  if (!specifics_string->empty()) {
    sync_pb::WifiConfigurationSpecificsData data;
    data.ParseFromString(*specifics_string);
    specifics = data;
  }
  base::Optional<int> completed_attempts =
      dict->FindIntPath(kCompletedAttemptsKey);

  DCHECK(change_guid);
  DCHECK(completed_attempts);

  return sync_wifi::PendingNetworkConfigurationUpdate(
      ssid, *change_guid, specifics, completed_attempts.value());
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
  dict_.SetPath(GeneratePath(ssid, kCompletedAttemptsKey), base::Value(0));
  pref_service_->Set(kPendingNetworkConfigurationsPref, dict_);
}

void PendingNetworkConfigurationTrackerImpl::MarkComplete(
    const std::string& change_guid,
    const std::string& ssid) {
  if (!GetPendingUpdate(change_guid, ssid))
    return;

  dict_.RemovePath(ssid);
  pref_service_->Set(kPendingNetworkConfigurationsPref, dict_);
}

void PendingNetworkConfigurationTrackerImpl::IncrementCompletedAttempts(
    const std::string& change_guid,
    const std::string& ssid) {
  std::string path = GeneratePath(ssid, kCompletedAttemptsKey);
  base::Optional<int> completed_attempts = dict_.FindIntPath(path);
  dict_.SetIntPath(path, completed_attempts.value() + 1);
}

std::vector<PendingNetworkConfigurationUpdate>
PendingNetworkConfigurationTrackerImpl::GetPendingUpdates() {
  std::vector<PendingNetworkConfigurationUpdate> list;
  for (const auto& entry : dict_.DictItems()) {
    list.push_back(
        ConvertToPendingUpdate(/*dict=*/&entry.second, /*ssid=*/entry.first));
  }
  return list;
}
base::Optional<PendingNetworkConfigurationUpdate>
PendingNetworkConfigurationTrackerImpl::GetPendingUpdate(
    const std::string& change_guid,
    const std::string& ssid) {
  std::string* found_id =
      dict_.FindStringPath(GeneratePath(ssid, kChangeGuidKey));
  if (!found_id || *found_id != change_guid)
    return base::nullopt;

  return ConvertToPendingUpdate(dict_.FindPath(ssid), ssid);
}

}  // namespace sync_wifi
