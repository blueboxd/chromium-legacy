// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_sender.h"

#include "chrome/browser/sharing/fcm_constants.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"

SharingFCMSender::SharingFCMSender(
    gcm::GCMDriver* gcm_driver,
    syncer::LocalDeviceInfoProvider* device_info_provider,
    SharingSyncPreference* sync_preference,
    VapidKeyManager* vapid_key_manager)
    : gcm_driver_(gcm_driver),
      device_info_provider_(device_info_provider),
      sync_preference_(sync_preference),
      vapid_key_manager_(vapid_key_manager) {}

SharingFCMSender::~SharingFCMSender() = default;

void SharingFCMSender::SendMessageToDevice(
    const std::string& device_guid,
    base::TimeDelta time_to_live,
    chrome_browser_sharing::SharingMessage message,
    SendMessageCallback callback) {
  auto synced_devices = sync_preference_->GetSyncedDevices();
  auto iter = synced_devices.find(device_guid);
  if (iter == synced_devices.end()) {
    LOG(ERROR) << "Unable to find device in preference";
    std::move(callback).Run(base::nullopt);
    return;
  }

  message.set_sender_guid(device_info_provider_->GetLocalDeviceInfo()->guid());

  gcm::WebPushMessage web_push_message;
  web_push_message.time_to_live = time_to_live.InSeconds();
  message.SerializeToString(&web_push_message.payload);

  gcm_driver_->SendWebPushMessage(
      kSharingFCMAppID,
      /* authorized_entity= */ "", iter->second.p256dh,
      iter->second.auth_secret, iter->second.fcm_token,
      vapid_key_manager_->GetOrCreateKey(), std::move(web_push_message),
      std::move(callback));
}
