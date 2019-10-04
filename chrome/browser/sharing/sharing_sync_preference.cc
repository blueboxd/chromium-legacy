// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_sync_preference.h"

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace {

const char kVapidECPrivateKey[] = "vapid_private_key";
const char kVapidCreationTimestamp[] = "vapid_creation_timestamp";

const char kDeviceFcmToken[] = "device_fcm_token";
const char kDeviceP256dh[] = "device_p256dh";
const char kDeviceAuthSecret[] = "device_auth_secret";
const char kDeviceCapabilities[] = "device_capabilities";

const char kRegistrationAuthorizedEntity[] = "registration_authorized_entity";
const char kRegistrationTimestamp[] = "registration_timestamp";

const char kSharingInfoFcmToken[] = "sharing_info_fcm_token";
const char kSharingInfoP256dh[] = "sharing_info_p256dh";
const char kSharingInfoAuthSecret[] = "sharing_info_auth_secret";
const char kSharingInfoEnabledFeatures[] = "sharing_info_enabled_features";

// Legacy value for enabled features on a device. These are stored in sync
// preferences when the device is registered, and the values should never be
// changed.
constexpr int kClickToCall = 1 << 0;
constexpr int kSharedClipboard = 1 << 1;

}  // namespace

using sync_pb::SharingSpecificFields;

SharingSyncPreference::FCMRegistration::FCMRegistration(
    std::string authorized_entity,
    base::Time timestamp)
    : authorized_entity(std::move(authorized_entity)), timestamp(timestamp) {}

SharingSyncPreference::FCMRegistration::FCMRegistration(
    FCMRegistration&& other) = default;

SharingSyncPreference::FCMRegistration& SharingSyncPreference::FCMRegistration::
operator=(FCMRegistration&& other) = default;

SharingSyncPreference::FCMRegistration::~FCMRegistration() = default;

SharingSyncPreference::SharingSyncPreference(
    PrefService* prefs,
    syncer::DeviceInfoSyncService* device_info_sync_service)
    : prefs_(prefs), device_info_sync_service_(device_info_sync_service) {
  DCHECK(prefs_);
  DCHECK(device_info_sync_service_);
  device_info_tracker_ = device_info_sync_service_->GetDeviceInfoTracker();
  local_device_info_provider_ =
      device_info_sync_service_->GetLocalDeviceInfoProvider();
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
  registry->RegisterDictionaryPref(prefs::kSharingLocalSharingInfo);
}

// static
base::Optional<syncer::DeviceInfo::SharingInfo>
SharingSyncPreference::GetLocalSharingInfoForSync(PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(kSharingUseDeviceInfo))
    return base::nullopt;

  return GetLocalSharingInfo(prefs);
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

base::Optional<SharingSyncPreference::FCMRegistration>
SharingSyncPreference::GetFCMRegistration() const {
  const base::DictionaryValue* registration =
      prefs_->GetDictionary(prefs::kSharingFCMRegistration);
  const std::string* authorized_entity =
      registration->FindStringKey(kRegistrationAuthorizedEntity);
  const base::Value* timestamp_value =
      registration->FindKey(kRegistrationTimestamp);
  if (!authorized_entity || !timestamp_value)
    return base::nullopt;

  base::Time timestamp;
  if (!base::GetValueAsTime(*timestamp_value, &timestamp))
    return base::nullopt;

  return FCMRegistration(*authorized_entity, timestamp);
}

void SharingSyncPreference::SetFCMRegistration(FCMRegistration registration) {
  DictionaryPrefUpdate update(prefs_, prefs::kSharingFCMRegistration);
  update->SetStringKey(kRegistrationAuthorizedEntity,
                       std::move(registration.authorized_entity));
  update->SetKey(kRegistrationTimestamp,
                 base::CreateTimeValue(registration.timestamp));
}

void SharingSyncPreference::ClearFCMRegistration() {
  prefs_->ClearPref(prefs::kSharingFCMRegistration);
}

base::Optional<syncer::DeviceInfo::SharingInfo>
SharingSyncPreference::GetSharingInfo(const std::string& guid) const {
  return GetSharingInfo(device_info_tracker_->GetDeviceInfo(guid).get());
}

base::Optional<syncer::DeviceInfo::SharingInfo>
SharingSyncPreference::GetSharingInfo(
    const syncer::DeviceInfo* device_info) const {
  if (!device_info)
    return base::nullopt;

  const base::Optional<syncer::DeviceInfo::SharingInfo>& sharing_info =
      device_info->sharing_info();
  if (sharing_info)
    return sharing_info;

  // Fallback to read from prefs::kSharingSyncedDevices if reading from sync
  // provider failed.
  const base::DictionaryValue* devices_preferences =
      prefs_->GetDictionary(prefs::kSharingSyncedDevices);
  const base::Value* value = devices_preferences->FindKey(device_info->guid());
  if (!value)
    return base::nullopt;

  return ValueToSharingInfo(*value);
}

base::Optional<syncer::DeviceInfo::SharingInfo>
SharingSyncPreference::GetLocalSharingInfo() const {
  auto* device_info = local_device_info_provider_->GetLocalDeviceInfo();
  if (!device_info)
    return base::nullopt;

  const base::Optional<syncer::DeviceInfo::SharingInfo>& sharing_info =
      device_info->sharing_info();
  if (sharing_info)
    return sharing_info;

  // Fallback to read from cached value (prefs::kSharingLocalSharingInfo).
  return GetLocalSharingInfo(prefs_);
}

void SharingSyncPreference::SetLocalSharingInfo(
    syncer::DeviceInfo::SharingInfo sharing_info) {
  auto* device_info = local_device_info_provider_->GetLocalDeviceInfo();
  if (!device_info)
    return;

  // Update prefs::kSharingSyncedDevices for legacy devices.
  DictionaryPrefUpdate synced_devices_update(prefs_,
                                             prefs::kSharingSyncedDevices);
  synced_devices_update->SetKey(device_info->guid(),
                                SharingInfoToValue(sharing_info));

  // Update prefs::kSharingLocalSharingInfo to cache value locally.
  if (device_info->sharing_info() == sharing_info)
    return;

  std::string base64_p256dh, base64_auth_secret;
  base::Base64Encode(sharing_info.p256dh, &base64_p256dh);
  base::Base64Encode(sharing_info.auth_secret, &base64_auth_secret);

  base::ListValue list_value;
  for (SharingSpecificFields::EnabledFeatures feature :
       sharing_info.enabled_features) {
    list_value.GetList().emplace_back(feature);
  }

  DictionaryPrefUpdate local_sharing_info_update(
      prefs_, prefs::kSharingLocalSharingInfo);
  local_sharing_info_update->SetStringKey(kSharingInfoFcmToken,
                                          std::move(sharing_info.fcm_token));
  local_sharing_info_update->SetStringKey(kSharingInfoP256dh,
                                          std::move(base64_p256dh));
  local_sharing_info_update->SetStringKey(kSharingInfoAuthSecret,
                                          std::move(base64_auth_secret));
  local_sharing_info_update->SetKey(kSharingInfoEnabledFeatures,
                                    std::move(list_value));

  if (base::FeatureList::IsEnabled(kSharingUseDeviceInfo))
    device_info_sync_service_->RefreshLocalDeviceInfo();
}

void SharingSyncPreference::ClearLocalSharingInfo() {
  auto* device_info = local_device_info_provider_->GetLocalDeviceInfo();
  if (!device_info)
    return;

  // Update prefs::kSharingSyncedDevices for legacy devices.
  DictionaryPrefUpdate update(prefs_, prefs::kSharingSyncedDevices);
  update->RemoveKey(device_info->guid());

  // Update prefs::kSharingLocalSharingInfo to clear local cache.
  prefs_->ClearPref(prefs::kSharingLocalSharingInfo);

  if (base::FeatureList::IsEnabled(kSharingUseDeviceInfo) &&
      device_info->sharing_info()) {
    device_info_sync_service_->RefreshLocalDeviceInfo();
  }
}

// static
base::Optional<syncer::DeviceInfo::SharingInfo>
SharingSyncPreference::GetLocalSharingInfo(PrefService* prefs) {
  const base::DictionaryValue* registration =
      prefs->GetDictionary(prefs::kSharingLocalSharingInfo);
  const std::string* fcm_token =
      registration->FindStringKey(kSharingInfoFcmToken);
  const std::string* base64_p256dh =
      registration->FindStringKey(kSharingInfoP256dh);
  const std::string* base64_auth_secret =
      registration->FindStringKey(kSharingInfoAuthSecret);
  const base::Value* enabled_features_value =
      registration->FindListKey(kSharingInfoEnabledFeatures);
  if (!fcm_token || !base64_p256dh || !base64_auth_secret ||
      !enabled_features_value) {
    return base::nullopt;
  }

  std::string p256dh, auth_secret;
  if (!base::Base64Decode(*base64_p256dh, &p256dh) ||
      !base::Base64Decode(*base64_auth_secret, &auth_secret)) {
    return base::nullopt;
  }

  std::set<SharingSpecificFields::EnabledFeatures> enabled_features;
  for (auto& value : enabled_features_value->GetList()) {
    if (!value.is_int())
      NOTREACHED();

    enabled_features.insert(
        static_cast<SharingSpecificFields::EnabledFeatures>(value.GetInt()));
  }

  return syncer::DeviceInfo::SharingInfo(*fcm_token, p256dh, auth_secret,
                                         std::move(enabled_features));
}

// static
base::Value SharingSyncPreference::SharingInfoToValue(
    const syncer::DeviceInfo::SharingInfo& device) {
  std::string base64_p256dh, base64_auth_secret;
  base::Base64Encode(device.p256dh, &base64_p256dh);
  base::Base64Encode(device.auth_secret, &base64_auth_secret);

  // For backward compatibility purposes, EnabledFeatures is converted to
  // bit-mask to be readable by legacy devices. New features won't be included.
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
base::Optional<syncer::DeviceInfo::SharingInfo>
SharingSyncPreference::ValueToSharingInfo(const base::Value& value) {
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

  // Legacy device store EnabledFeatures as bit-mask. They won't support new
  // features.
  std::set<SharingSpecificFields::EnabledFeatures> enabled_features;
  if ((*capabilities & kClickToCall) == kClickToCall)
    enabled_features.insert(SharingSpecificFields::CLICK_TO_CALL);
  if ((*capabilities & kSharedClipboard) == kSharedClipboard)
    enabled_features.insert(SharingSpecificFields::SHARED_CLIPBOARD);

  return syncer::DeviceInfo::SharingInfo(*fcm_token, std::move(p256dh),
                                         std::move(auth_secret),
                                         std::move(enabled_features));
}
