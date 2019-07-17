// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_sync_preference.h"

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace {

const char kVapidECPrivateKey[] = "vapid_private_key";
const char kVapidCreationTimestamp[] = "vapid_creation_timestamp";

const char kDeviceFcmToken[] = "device_fcm_token";
const char kDeviceP256dh[] = "device_p256dh";
const char kDeviceAuthSecret[] = "device_auth_secret";
const char kDeviceCapabilities[] = "device_capabilities";
const char kDeviceLastUpdated[] = "device_last_updated";

const char kRegistrationAuthorizedEntity[] = "registration_authorized_entity";
const char kRegistrationFcmToken[] = "registration_fcm_token";
const char kRegistrationTimestamp[] = "registration_timestamp";

base::Time GetTimestamp(const base::Value& value, base::StringPiece key) {
  if (!value.is_dict())
    return base::Time();
  base::Time timestamp;
  auto* timestamp_value = value.FindKey(key);
  if (!timestamp_value || !base::GetValueAsTime(*timestamp_value, &timestamp))
    return base::Time();
  return timestamp;
}

bool ShouldUseLocalVapidKey(const base::Value& local_value,
                            const base::Value& server_value) {
  auto local_timestamp = GetTimestamp(local_value, kVapidCreationTimestamp);
  bool has_local_timestamp = !local_timestamp.is_null();
  if (!has_local_timestamp)
    return false;

  auto server_timestamp = GetTimestamp(server_value, kVapidCreationTimestamp);
  bool has_server_timestamp = !server_timestamp.is_null();
  if (!has_server_timestamp)
    return true;

  // Use older VAPID key if two versions exist to reduce the number of FCM
  // registration invalidations as only new devices would encounter this.
  return local_timestamp < server_timestamp;
}

bool ShouldUseLocalDeviceData(const base::Value& local_value,
                              const base::Value& server_value) {
  auto local_timestamp = GetTimestamp(local_value, kDeviceLastUpdated);
  bool has_local_timestamp = !local_timestamp.is_null();
  if (!has_local_timestamp)
    return false;

  auto server_timestamp = GetTimestamp(server_value, kDeviceLastUpdated);
  bool has_server_timestamp = !server_timestamp.is_null();
  if (!has_server_timestamp)
    return true;

  // Use newer device data if two versions exist. We guarantee that only the
  // same device updates its own data, so clock diff issues should be minimal.
  return local_timestamp > server_timestamp;
}

}  // namespace

SharingSyncPreference::Device::Device(std::string fcm_token,
                                      std::string p256dh,
                                      std::string auth_secret,
                                      const int capabilities)
    : fcm_token(std::move(fcm_token)),
      p256dh(std::move(p256dh)),
      auth_secret(std::move(auth_secret)),
      capabilities(capabilities) {}

SharingSyncPreference::Device::Device(Device&& other) = default;

SharingSyncPreference::Device::~Device() = default;

SharingSyncPreference::SharingSyncPreference(PrefService* prefs)
    : prefs_(prefs) {
  pref_change_registrar_.Init(prefs);
}

SharingSyncPreference::~SharingSyncPreference() = default;

// static
void SharingSyncPreference::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kSharingSyncedDevices,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kSharingVapidKey, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kSharingFCMRegistration);
}

// static
std::unique_ptr<base::Value> SharingSyncPreference::MaybeMergeVapidKey(
    const base::Value& local_value,
    const base::Value& server_value) {
  return ShouldUseLocalVapidKey(local_value, server_value)
             ? base::Value::ToUniquePtrValue(local_value.Clone())
             : nullptr;
}

// static
std::unique_ptr<base::Value> SharingSyncPreference::MaybeMergeSyncedDevices(
    const base::Value& local_value,
    const base::Value& server_value) {
  if (!local_value.is_dict() || !server_value.is_dict())
    return nullptr;

  base::Value local_overrides(base::Value::Type::DICTIONARY);
  for (const auto& it : local_value.DictItems()) {
    const std::string& device_guid = it.first;
    const base::Value& local = it.second;
    const base::Value* server = server_value.FindKey(device_guid);
    if (!server || ShouldUseLocalDeviceData(local, *server))
      local_overrides.SetKey(device_guid, local.Clone());
  }

  if (local_overrides.DictEmpty())
    return nullptr;

  base::Value result = server_value.Clone();
  result.MergeDictionary(&local_overrides);
  return base::Value::ToUniquePtrValue(std::move(result));
}

base::Optional<std::vector<uint8_t>> SharingSyncPreference::GetVapidKey()
    const {
  const base::DictionaryValue* vapid_key =
      prefs_->GetDictionary(prefs::kSharingVapidKey);
  std::string base64_private_key, private_key;
  if (!vapid_key->GetString(kVapidECPrivateKey, &base64_private_key))
    return base::nullopt;

  if (base::Base64Decode(base64_private_key, &private_key)) {
    return std::vector<uint8_t>(private_key.begin(), private_key.end());
  } else {
    LOG(ERROR) << "Could not decode stored vapid keys.";
    return base::nullopt;
  }
}

void SharingSyncPreference::SetVapidKey(
    const std::vector<uint8_t>& vapid_key) const {
  base::Time creation_timestamp = base::Time::Now();
  std::string base64_vapid_key;
  base::Base64Encode(std::string(vapid_key.begin(), vapid_key.end()),
                     &base64_vapid_key);

  DictionaryPrefUpdate update(prefs_, prefs::kSharingVapidKey);
  update->SetString(kVapidECPrivateKey, base64_vapid_key);
  update->SetString(kVapidCreationTimestamp,
                    base::CreateTimeValue(creation_timestamp).GetString());
}

void SharingSyncPreference::SetVapidKeyChangeObserver(
    const base::RepeatingClosure& obs) {
  ClearVapidKeyChangeObserver();
  pref_change_registrar_.Add(prefs::kSharingVapidKey, obs);
}

void SharingSyncPreference::ClearVapidKeyChangeObserver() {
  if (pref_change_registrar_.IsObserved(prefs::kSharingVapidKey))
    pref_change_registrar_.Remove(prefs::kSharingVapidKey);
}

std::map<std::string, SharingSyncPreference::Device>
SharingSyncPreference::GetSyncedDevices() const {
  std::map<std::string, Device> synced_devices;
  const base::DictionaryValue* devices_preferences =
      prefs_->GetDictionary(prefs::kSharingSyncedDevices);
  for (const auto& it : devices_preferences->DictItems()) {
    base::Optional<Device> device = ValueToDevice(it.second);
    if (device)
      synced_devices.emplace(it.first, std::move(*device));
  }
  return synced_devices;
}

void SharingSyncPreference::SetSyncDevice(const std::string& guid,
                                          const Device& device) {
  DictionaryPrefUpdate update(prefs_, prefs::kSharingSyncedDevices);
  update->SetKey(guid, DeviceToValue(device, base::Time::Now()));
}

void SharingSyncPreference::RemoveDevice(const std::string& guid) {
  DictionaryPrefUpdate update(prefs_, prefs::kSharingSyncedDevices);
  // Clear all values of device with |guid| by setting its value to an empty
  // entry that only contains a timestamp so other devices can merge it.
  base::Value cleared(base::Value::Type::DICTIONARY);
  cleared.SetKey(kDeviceLastUpdated, base::CreateTimeValue(base::Time::Now()));
  update->SetKey(guid, std::move(cleared));
}

base::Optional<SharingSyncPreference::FCMRegistration>
SharingSyncPreference::GetFCMRegistration() const {
  const base::DictionaryValue* registration =
      prefs_->GetDictionary(prefs::kSharingFCMRegistration);
  const std::string* authorized_entity =
      registration->FindStringPath(kRegistrationAuthorizedEntity);
  const std::string* fcm_token =
      registration->FindStringPath(kRegistrationFcmToken);
  const base::Value* timestamp_value =
      registration->FindPath(kRegistrationTimestamp);
  if (!authorized_entity || !fcm_token || !timestamp_value)
    return base::nullopt;

  base::Time timestamp;
  if (!base::GetValueAsTime(*timestamp_value, &timestamp))
    return base::nullopt;

  return FCMRegistration{*authorized_entity, *fcm_token, timestamp};
}

void SharingSyncPreference::SetFCMRegistration(FCMRegistration registration) {
  DictionaryPrefUpdate update(prefs_, prefs::kSharingFCMRegistration);
  update->SetStringKey(kRegistrationAuthorizedEntity,
                       std::move(registration.authorized_entity));
  update->SetStringKey(kRegistrationFcmToken,
                       std::move(registration.fcm_token));
  update->SetKey(kRegistrationTimestamp,
                 base::CreateTimeValue(registration.timestamp));
}

void SharingSyncPreference::ClearFCMRegistration() {
  prefs_->ClearPref(prefs::kSharingFCMRegistration);
}

// static
base::Value SharingSyncPreference::DeviceToValue(const Device& device,
                                                 base::Time timestamp) {
  std::string base64_p256dh, base64_auth_secret;
  base::Base64Encode(device.p256dh, &base64_p256dh);
  base::Base64Encode(device.auth_secret, &base64_auth_secret);

  base::Value result(base::Value::Type::DICTIONARY);
  result.SetStringKey(kDeviceFcmToken, device.fcm_token);
  result.SetStringKey(kDeviceP256dh, base64_p256dh);
  result.SetStringKey(kDeviceAuthSecret, base64_auth_secret);
  result.SetIntKey(kDeviceCapabilities, device.capabilities);
  result.SetKey(kDeviceLastUpdated, base::CreateTimeValue(timestamp));
  return result;
}

// static
base::Optional<SharingSyncPreference::Device>
SharingSyncPreference::ValueToDevice(const base::Value& value) {
  const std::string* fcm_token = value.FindStringKey(kDeviceFcmToken);
  if (!fcm_token)
    return base::nullopt;

  const std::string* base64_p256dh = value.FindStringKey(kDeviceP256dh);
  const std::string* base64_auth_secret =
      value.FindStringKey(kDeviceAuthSecret);
  const base::Optional<int> capabilities =
      value.FindIntKey(kDeviceCapabilities);

  std::string p256dh, auth_secret;
  if (!base64_p256dh || !base64_auth_secret || !capabilities ||
      !base::Base64Decode(*base64_p256dh, &p256dh) ||
      !base::Base64Decode(*base64_auth_secret, &auth_secret)) {
    LOG(ERROR) << "Could not convert synced value to device object.";
    return base::nullopt;
  }

  return Device(*fcm_token, std::move(p256dh), std::move(auth_secret),
                *capabilities);
}
