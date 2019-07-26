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
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "crypto/ec_private_key.h"

#if defined(OS_ANDROID)
#include "chrome/android/chrome_jni_headers/SharingJNIBridge_jni.h"
#endif

SharingDeviceRegistration::SharingDeviceRegistration(
    SharingSyncPreference* sharing_sync_preference,
    instance_id::InstanceIDDriver* instance_id_driver,
    VapidKeyManager* vapid_key_manager,
    syncer::LocalDeviceInfoProvider* local_device_info_provider)
    : sharing_sync_preference_(sharing_sync_preference),
      instance_id_driver_(instance_id_driver),
      vapid_key_manager_(vapid_key_manager),
      local_device_info_provider_(local_device_info_provider) {}

SharingDeviceRegistration::~SharingDeviceRegistration() = default;

void SharingDeviceRegistration::RegisterDevice(RegistrationCallback callback) {
  base::Optional<std::string> authorized_entity = GetAuthorizationEntity();
  if (!authorized_entity) {
    std::move(callback).Run(SharingDeviceRegistrationResult::kEncryptionError);
    return;
  }

  auto registration = sharing_sync_preference_->GetFCMRegistration();
  if (registration && registration->authorized_entity == authorized_entity &&
      (base::Time::Now() - registration->timestamp < kRegistrationExpiration)) {
    // Authorized entity hasn't changed nor has expired, skip to next step.
    RetrieveEncryptionInfo(std::move(callback), registration->authorized_entity,
                           registration->fcm_token);
    return;
  }

  instance_id_driver_->GetInstanceID(kSharingFCMAppID)
      ->GetToken(*authorized_entity, kFCMScope,
                 /*options=*/{},
                 /*is_lazy=*/false,
                 base::BindOnce(&SharingDeviceRegistration::OnFCMTokenReceived,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(callback), *authorized_entity));
}

void SharingDeviceRegistration::OnFCMTokenReceived(
    RegistrationCallback callback,
    const std::string& authorized_entity,
    const std::string& fcm_registration_token,
    instance_id::InstanceID::Result result) {
  switch (result) {
    case instance_id::InstanceID::SUCCESS:
      sharing_sync_preference_->SetFCMRegistration(
          {authorized_entity, fcm_registration_token, base::Time::Now()});
      RetrieveEncryptionInfo(std::move(callback), authorized_entity,
                             fcm_registration_token);
      break;
    case instance_id::InstanceID::NETWORK_ERROR:
    case instance_id::InstanceID::SERVER_ERROR:
    case instance_id::InstanceID::ASYNC_OPERATION_PENDING:
      std::move(callback).Run(
          SharingDeviceRegistrationResult::kFcmTransientError);
      break;
    case instance_id::InstanceID::INVALID_PARAMETER:
    case instance_id::InstanceID::UNKNOWN_ERROR:
    case instance_id::InstanceID::DISABLED:
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
                         fcm_registration_token));
}

void SharingDeviceRegistration::OnEncryptionInfoReceived(
    RegistrationCallback callback,
    const std::string& fcm_registration_token,
    std::string p256dh,
    std::string auth_secret) {
  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  if (!local_device_info) {
    std::move(callback).Run(SharingDeviceRegistrationResult::kSyncServiceError);
    return;
  }

  int device_capabilities = GetDeviceCapabilities();
  SharingSyncPreference::Device device(
      fcm_registration_token, std::move(p256dh), std::move(auth_secret),
      device_capabilities);
  sharing_sync_preference_->SetSyncDevice(local_device_info->guid(), device);
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

  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  if (local_device_info)
    sharing_sync_preference_->RemoveDevice(local_device_info->guid());

  instance_id_driver_->GetInstanceID(kSharingFCMAppID)
      ->DeleteToken(
          registration->authorized_entity, kFCMScope,
          base::BindOnce(&SharingDeviceRegistration::OnFCMTokenDeleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SharingDeviceRegistration::OnFCMTokenDeleted(
    RegistrationCallback callback,
    instance_id::InstanceID::Result result) {
  switch (result) {
    case instance_id::InstanceID::SUCCESS:
      // INVALID_PARAMETER is expected if InstanceID.GetToken hasn't been
      // invoked since restart.
    case instance_id::InstanceID::INVALID_PARAMETER:
      sharing_sync_preference_->ClearFCMRegistration();
      std::move(callback).Run(SharingDeviceRegistrationResult::kSuccess);
      return;
    case instance_id::InstanceID::NETWORK_ERROR:
    case instance_id::InstanceID::SERVER_ERROR:
    case instance_id::InstanceID::ASYNC_OPERATION_PENDING:
      std::move(callback).Run(
          SharingDeviceRegistrationResult::kFcmTransientError);
      return;
    case instance_id::InstanceID::UNKNOWN_ERROR:
    case instance_id::InstanceID::DISABLED:
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

int SharingDeviceRegistration::GetDeviceCapabilities() const {
  int device_capabilities = static_cast<int>(SharingDeviceCapability::kNone);
  if (IsTelephonySupported()) {
    device_capabilities |=
        static_cast<int>(SharingDeviceCapability::kTelephony);
  }
  return device_capabilities;
}

bool SharingDeviceRegistration::IsTelephonySupported() const {
#if defined(OS_ANDROID)
  if (!base::FeatureList::IsEnabled(kClickToCallReceiver))
    return false;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_SharingJNIBridge_isTelephonySupported(env);
#endif

  return false;
}
