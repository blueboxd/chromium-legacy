// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_sync_preference.h"

#include "base/base64.h"
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

const char kRegistrationAuthorizedEntity[] = "registration_authorized_entity";
const char kRegistrationFcmToken[] = "registration_fcm_token";
const char kRegistrationP256dh[] = "registration_p256dh";
const char kRegistrationAuthSecret[] = "registration_auth_secret";
const char kRegistrationTimestamp[] = "registration_timestamp";

// Legacy value for enabled features on a device. These are stored in sync
// preferences when the device is registered, and the values should never be
// changed.
constexpr int kClickToCall = 1 << 0;
constexpr int kSharedClipboard = 1 << 1;

}  // namespace

using sync_pb::SharingSpecificFields;

SharingSyncPreference::Device::Device(
    std::string fcm_token,
    std::string p256dh,
    std::string auth_secret,
    std::set<SharingSpecificFields::EnabledFeatures> enabled_features)
    : fcm_token(std::move(fcm_token)),
      p256dh(std::move(p256dh)),
      auth_secret(std::move(auth_secret)),
      enabled_features(std::move(enabled_features)) {}

SharingSyncPreference::Device::Device(Device&& other) = default;

SharingSyncPreference::Device& SharingSyncPreference::Device::operator=(
    Device&& other) = default;

SharingSyncPreference::Device::~Device() = default;

SharingSyncPreference::FCMRegistration::FCMRegistration(
    std::string authorized_entity,
    std::string fcm_token,
    std::string p256dh,
    std::string auth_secret,
    base::Time timestamp)
    : authorized_entity(std::move(authorized_entity)),
      fcm_token(std::move(fcm_token)),
      p256dh(std::move(p256dh)),
      auth_secret(std::move(auth_secret)),
      timestamp(timestamp) {}

SharingSyncPreference::FCMRegistration::FCMRegistration(
    FCMRegistration&& other) = default;

SharingSyncPreference::FCMRegistration& SharingSyncPreference::FCMRegistration::
operator=(FCMRegistration&& other) = default;

SharingSyncPreference::FCMRegistration::~FCMRegistration() = default;

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

base::Optional<SharingSyncPreference::Device>
SharingSyncPreference::GetSyncedDevice(const std::string& guid) const {
  const base::DictionaryValue* devices_preferences =
      prefs_->GetDictionary(prefs::kSharingSyncedDevices);
  const base::Value* value = devices_preferences->FindKey(guid);
  if (!value)
    return base::nullopt;

  return ValueToDevice(*value);
}

void SharingSyncPreference::SetSyncDevice(const std::string& guid,
                                          const Device& device) {
  DictionaryPrefUpdate update(prefs_, prefs::kSharingSyncedDevices);
  update->SetKey(guid, DeviceToValue(device));
}

void SharingSyncPreference::RemoveDevice(const std::string& guid) {
  DictionaryPrefUpdate update(prefs_, prefs::kSharingSyncedDevices);
  update->RemoveKey(guid);
}

base::Optional<SharingSyncPreference::FCMRegistration>
SharingSyncPreference::GetFCMRegistration() const {
  const base::DictionaryValue* registration =
      prefs_->GetDictionary(prefs::kSharingFCMRegistration);
  const std::string* authorized_entity =
      registration->FindStringKey(kRegistrationAuthorizedEntity);
  const std::string* fcm_token =
      registration->FindStringKey(kRegistrationFcmToken);
  const std::string* base64_p256dh =
      registration->FindStringKey(kRegistrationP256dh);
  const std::string* base64_auth_secret =
      registration->FindStringKey(kRegistrationAuthSecret);
  const base::Value* timestamp_value =
      registration->FindKey(kRegistrationTimestamp);
  if (!authorized_entity || !fcm_token || !base64_p256dh ||
      !base64_auth_secret || !timestamp_value) {
    return base::nullopt;
  }

  std::string p256dh, auth_secret;
  base::Time timestamp;
  if (!base::Base64Decode(*base64_p256dh, &p256dh) ||
      !base::Base64Decode(*base64_auth_secret, &auth_secret) ||
      !base::GetValueAsTime(*timestamp_value, &timestamp)) {
    return base::nullopt;
  }

  return FCMRegistration(*authorized_entity, *fcm_token, p256dh, auth_secret,
                         timestamp);
}

void SharingSyncPreference::SetFCMRegistration(FCMRegistration registration) {
  std::string base64_p256dh, base64_auth_secret;
  base::Base64Encode(registration.p256dh, &base64_p256dh);
  base::Base64Encode(registration.auth_secret, &base64_auth_secret);

  DictionaryPrefUpdate update(prefs_, prefs::kSharingFCMRegistration);
  update->SetStringKey(kRegistrationAuthorizedEntity,
                       std::move(registration.authorized_entity));
  update->SetStringKey(kRegistrationFcmToken,
                       std::move(registration.fcm_token));
  update->SetStringKey(kRegistrationP256dh, std::move(base64_p256dh));
  update->SetStringKey(kRegistrationAuthSecret, std::move(base64_auth_secret));
  update->SetKey(kRegistrationTimestamp,
                 base::CreateTimeValue(registration.timestamp));
}

void SharingSyncPreference::ClearFCMRegistration() {
  prefs_->ClearPref(prefs::kSharingFCMRegistration);
}

// static
base::Value SharingSyncPreference::DeviceToValue(const Device& device) {
  std::string base64_p256dh, base64_auth_secret;
  base::Base64Encode(device.p256dh, &base64_p256dh);
  base::Base64Encode(device.auth_secret, &base64_auth_secret);

  int capabilities = 0;
  if (device.enabled_features.count(SharingSpecificFields::CLICK_TO_CALL))
    capabilities |= kClickToCall;
  if (device.enabled_features.count(SharingSpecificFields::SHARED_CLIPBOARD))
    capabilities |= kSharedClipboard;

  base::Value result(base::Value::Type::DICTIONARY);
  result.SetStringKey(kDeviceFcmToken, device.fcm_token);
  result.SetStringKey(kDeviceP256dh, base64_p256dh);
  result.SetStringKey(kDeviceAuthSecret, base64_auth_secret);
  result.SetIntKey(kDeviceCapabilities, capabilities);
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

  std::set<SharingSpecificFields::EnabledFeatures> enabled_features;
  if ((*capabilities & kClickToCall) == kClickToCall)
    enabled_features.insert(SharingSpecificFields::CLICK_TO_CALL);
  if ((*capabilities & kSharedClipboard) == kSharedClipboard)
    enabled_features.insert(SharingSpecificFields::SHARED_CLIPBOARD);

  return Device(*fcm_token, std::move(p256dh), std::move(auth_secret),
                std::move(enabled_features));
}
