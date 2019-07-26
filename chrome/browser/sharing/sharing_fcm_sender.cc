// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_sender.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/sync_device_info/device_info.h"

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

  auto send_message_closure =
      base::BindOnce(&SharingFCMSender::DoSendMessageToDevice,
                     weak_ptr_factory_.GetWeakPtr(), std::move(iter->second),
                     time_to_live, std::move(message), std::move(callback));

  if (device_info_provider_->GetLocalDeviceInfo()) {
    std::move(send_message_closure).Run();
    return;
  }

  if (!local_device_info_ready_subscription_) {
    local_device_info_ready_subscription_ =
        device_info_provider_->RegisterOnInitializedCallback(
            base::AdaptCallbackForRepeating(
                base::BindOnce(&SharingFCMSender::OnLocalDeviceInfoInitialized,
                               weak_ptr_factory_.GetWeakPtr())));
  }

  send_message_queue_.emplace_back(std::move(send_message_closure));
}

void SharingFCMSender::OnLocalDeviceInfoInitialized() {
  for (auto& closure : send_message_queue_)
    std::move(closure).Run();
  send_message_queue_.clear();
  local_device_info_ready_subscription_.reset();
}

void SharingFCMSender::DoSendMessageToDevice(
    SharingSyncPreference::Device target,
    base::TimeDelta time_to_live,
    chrome_browser_sharing::SharingMessage message,
    SendMessageCallback callback) {
  message.set_sender_guid(device_info_provider_->GetLocalDeviceInfo()->guid());

  gcm::WebPushMessage web_push_message;
  web_push_message.time_to_live = time_to_live.InSeconds();
  web_push_message.urgency = gcm::WebPushMessage::Urgency::kHigh;
  message.SerializeToString(&web_push_message.payload);

  auto fcm_registration = sync_preference_->GetFCMRegistration();
  if (!fcm_registration) {
    LOG(ERROR) << "Unable to retrieve FCM registration";
    std::move(callback).Run(base::nullopt);
    return;
  }

  gcm_driver_->SendWebPushMessage(
      kSharingFCMAppID, fcm_registration->authorized_entity, target.p256dh,
      target.auth_secret, target.fcm_token,
      vapid_key_manager_->GetOrCreateKey(), std::move(web_push_message),
      std::move(callback));
}
