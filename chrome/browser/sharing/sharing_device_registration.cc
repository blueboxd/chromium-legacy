// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_device_registration.h"

#include <stdint.h>
#include <vector>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/optional.h"
#include "chrome/browser/sharing/fcm_constants.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "crypto/ec_private_key.h"

SharingDeviceRegistration::SharingDeviceRegistration(
    SharingSyncPreference* sharing_sync_preference,
    instance_id::InstanceIDDriver* instance_id_driver,
    VapidKeyManager* vapid_key_manager,
    gcm::GCMDriver* gcm_driver,
    syncer::LocalDeviceInfoProvider* local_device_info_provider)
    : sharing_sync_preference_(sharing_sync_preference),
      instance_id_driver_(instance_id_driver),
      vapid_key_manager_(vapid_key_manager),
      gcm_driver_(gcm_driver),
      local_device_info_provider_(local_device_info_provider),
      weak_ptr_factory_(this) {}

SharingDeviceRegistration::~SharingDeviceRegistration() = default;

void SharingDeviceRegistration::RegisterDevice(RegistrationCallback callback) {
  // TODO(himanshujaju) : Extract a static function to convert ECPrivateKey* to
  // Base64PublicKey in library.
  crypto::ECPrivateKey* vapid_key = vapid_key_manager_->GetOrCreateKey();
  if (!vapid_key) {
    std::move(callback).Run(Result::FAILURE);
    return;
  }

  std::string public_key;
  if (!gcm::GetRawPublicKey(*vapid_key, &public_key)) {
    std::move(callback).Run(Result::FAILURE);
    return;
  }

  std::string base64_public_key;
  base::Base64UrlEncode(public_key, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base64_public_key);

  instance_id_driver_->GetInstanceID(kSharingFCMAppID)
      ->GetToken(
          base64_public_key, kFCMScope,
          /*options=*/{},
          /*is_lazy=*/false,
          base::BindOnce(&SharingDeviceRegistration::OnFCMTokenReceived,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SharingDeviceRegistration::OnFCMTokenReceived(
    RegistrationCallback callback,
    const std::string& fcm_registration_token,
    instance_id::InstanceID::Result result) {
  if (result != instance_id::InstanceID::SUCCESS) {
    std::move(callback).Run(Result::FAILURE);
    return;
  }

  gcm_driver_->GetEncryptionInfo(
      kSharingFCMAppID,
      base::AdaptCallbackForRepeating(
          base::BindOnce(&SharingDeviceRegistration::OnEncryptionInfoReceived,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         fcm_registration_token)));
}

void SharingDeviceRegistration::OnEncryptionInfoReceived(
    RegistrationCallback callback,
    const std::string& fcm_registration_token,
    const std::string& p256dh,
    const std::string& auth_secret) const {
  int device_capabilities = GetDeviceCapabilities();
  std::string device_guid =
      local_device_info_provider_->GetLocalDeviceInfo()->guid();
  SharingSyncPreference::Device device(fcm_registration_token, p256dh,
                                       auth_secret, device_capabilities);

  sharing_sync_preference_->SetSyncDevice(device_guid, device);
  std::move(callback).Run(Result::SUCCESS);
}

int SharingDeviceRegistration::GetDeviceCapabilities() const {
  int device_capabilities = static_cast<int>(SharingDeviceCapability::kNone);
  return device_capabilities;
}
