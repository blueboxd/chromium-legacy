// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_device_registration.h"

#include <stdint.h>
#include <vector>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/common/pref_names.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info.h"
#include "crypto/ec_private_key.h"

#if defined(OS_ANDROID)
#include "chrome/android/chrome_jni_headers/SharingJNIBridge_jni.h"
#endif

using instance_id::InstanceID;
using sync_pb::SharingSpecificFields;

SharingDeviceRegistration::SharingDeviceRegistration(
    PrefService* pref_service,
    SharingSyncPreference* sharing_sync_preference,
    instance_id::InstanceIDDriver* instance_id_driver,
    VapidKeyManager* vapid_key_manager)
    : pref_service_(pref_service),
      sharing_sync_preference_(sharing_sync_preference),
      instance_id_driver_(instance_id_driver),
      vapid_key_manager_(vapid_key_manager) {}

SharingDeviceRegistration::~SharingDeviceRegistration() = default;

void SharingDeviceRegistration::RegisterDevice(RegistrationCallback callback) {
  base::Optional<std::string> authorized_entity = GetAuthorizationEntity();
  if (!authorized_entity) {
    std::move(callback).Run(SharingDeviceRegistrationResult::kEncryptionError);
    return;
  }

  base::Optional<SharingSyncPreference::FCMRegistration> registration =
      sharing_sync_preference_->GetFCMRegistration();
  base::Optional<syncer::DeviceInfo::SharingInfo> sharing_info =
      sharing_sync_preference_->GetLocalSharingInfo();
  if (registration && registration->authorized_entity == authorized_entity &&
      (base::Time::Now() - registration->timestamp < kRegistrationExpiration) &&
      sharing_info) {
    // Authorized entity hasn't changed nor has expired, skip to next step.
    RetrieveEncryptionInfo(std::move(callback), registration->authorized_entity,
                           sharing_info->fcm_token);
    return;
  }

  instance_id_driver_->GetInstanceID(kSharingFCMAppID)
      ->GetToken(*authorized_entity, kFCMScope,
                 /*options=*/{},
                 /*flags=*/{InstanceID::Flags::kBypassScheduler},
                 base::BindOnce(&SharingDeviceRegistration::OnFCMTokenReceived,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(callback), *authorized_entity));
}

void SharingDeviceRegistration::OnFCMTokenReceived(
    RegistrationCallback callback,
    const std::string& authorized_entity,
    const std::string& fcm_registration_token,
    InstanceID::Result result) {
  switch (result) {
    case InstanceID::SUCCESS:
      RetrieveEncryptionInfo(std::move(callback), authorized_entity,
                             fcm_registration_token);
      break;
    case InstanceID::NETWORK_ERROR:
    case InstanceID::SERVER_ERROR:
    case InstanceID::ASYNC_OPERATION_PENDING:
      std::move(callback).Run(
          SharingDeviceRegistrationResult::kFcmTransientError);
      break;
    case InstanceID::INVALID_PARAMETER:
    case InstanceID::UNKNOWN_ERROR:
    case InstanceID::DISABLED:
      std::move(callback).Run(SharingDeviceRegistrationResult::kFcmFatalError);
      break;
  }
}

void SharingDeviceRegistration::RetrieveEncryptionInfo(
    RegistrationCallback callback,
    const std::string& authorized_entity,
    const std::string& fcm_registration_token) {
  instance_id_driver_->GetInstanceID(kSharingFCMAppID)
      ->GetEncryptionInfo(
          authorized_entity,
          base::BindOnce(&SharingDeviceRegistration::OnEncryptionInfoReceived,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         authorized_entity, fcm_registration_token));
}

void SharingDeviceRegistration::OnEncryptionInfoReceived(
    RegistrationCallback callback,
    const std::string& authorized_entity,
    const std::string& fcm_registration_token,
    std::string p256dh,
    std::string auth_secret) {
  sharing_sync_preference_->SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(authorized_entity,
                                             base::Time::Now()));

  std::set<SharingSpecificFields::EnabledFeatures> enabled_features =
      GetEnabledFeatures();
  syncer::DeviceInfo::SharingInfo sharing_info(
      fcm_registration_token, std::move(p256dh), std::move(auth_secret),
      enabled_features);
  sharing_sync_preference_->SetLocalSharingInfo(std::move(sharing_info));
  std::move(callback).Run(SharingDeviceRegistrationResult::kSuccess);
}

void SharingDeviceRegistration::UnregisterDevice(
    RegistrationCallback callback) {
  auto registration = sharing_sync_preference_->GetFCMRegistration();
  if (!registration) {
    std::move(callback).Run(
        SharingDeviceRegistrationResult::kDeviceNotRegistered);
    return;
  }

  sharing_sync_preference_->ClearLocalSharingInfo();

  instance_id_driver_->GetInstanceID(kSharingFCMAppID)
      ->DeleteToken(
          registration->authorized_entity, kFCMScope,
          base::BindOnce(&SharingDeviceRegistration::OnFCMTokenDeleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SharingDeviceRegistration::OnFCMTokenDeleted(RegistrationCallback callback,
                                                  InstanceID::Result result) {
  switch (result) {
    case InstanceID::SUCCESS:
      // INVALID_PARAMETER is expected if InstanceID.GetToken hasn't been
      // invoked since restart.
    case InstanceID::INVALID_PARAMETER:
      sharing_sync_preference_->ClearFCMRegistration();
      std::move(callback).Run(SharingDeviceRegistrationResult::kSuccess);
      return;
    case InstanceID::NETWORK_ERROR:
    case InstanceID::SERVER_ERROR:
    case InstanceID::ASYNC_OPERATION_PENDING:
      std::move(callback).Run(
          SharingDeviceRegistrationResult::kFcmTransientError);
      return;
    case InstanceID::UNKNOWN_ERROR:
    case InstanceID::DISABLED:
      std::move(callback).Run(SharingDeviceRegistrationResult::kFcmFatalError);
      return;
  }

  NOTREACHED();
}

base::Optional<std::string> SharingDeviceRegistration::GetAuthorizationEntity()
    const {
  // TODO(himanshujaju) : Extract a static function to convert ECPrivateKey* to
  // Base64PublicKey in library.
  crypto::ECPrivateKey* vapid_key = vapid_key_manager_->GetOrCreateKey();
  if (!vapid_key)
    return base::nullopt;

  std::string public_key;
  if (!gcm::GetRawPublicKey(*vapid_key, &public_key))
    return base::nullopt;

  std::string base64_public_key;
  base::Base64UrlEncode(public_key, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base64_public_key);
  return base::make_optional(std::move(base64_public_key));
}

std::set<SharingSpecificFields::EnabledFeatures>
SharingDeviceRegistration::GetEnabledFeatures() const {
  // Used in tests
  if (enabled_features_testing_value_)
    return enabled_features_testing_value_.value();

  std::set<SharingSpecificFields::EnabledFeatures> enabled_features;
  if (IsClickToCallSupported())
    enabled_features.insert(SharingSpecificFields::CLICK_TO_CALL);
  if (IsSharedClipboardSupported())
    enabled_features.insert(SharingSpecificFields::SHARED_CLIPBOARD);

  return enabled_features;
}

bool SharingDeviceRegistration::IsClickToCallSupported() const {
#if defined(OS_ANDROID)
  if (!base::FeatureList::IsEnabled(kClickToCallReceiver))
    return false;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_SharingJNIBridge_isTelephonySupported(env);
#endif

  return false;
}

bool SharingDeviceRegistration::IsSharedClipboardSupported() const {
  // Check the enterprise policy for Shared Clipboard.
  if (pref_service_ &&
      !pref_service_->GetBoolean(prefs::kSharedClipboardEnabled)) {
    return false;
  }
  return base::FeatureList::IsEnabled(kSharedClipboardReceiver);
}

void SharingDeviceRegistration::SetEnabledFeaturesForTesting(
    std::set<SharingSpecificFields::EnabledFeatures> enabled_features) {
  enabled_features_testing_value_ = std::move(enabled_features);
}
