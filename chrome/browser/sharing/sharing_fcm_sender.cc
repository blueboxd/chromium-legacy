// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_sender.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
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
    syncer::DeviceInfo::SharingInfo target,
    base::TimeDelta time_to_live,
    SharingMessage message,
    SendMessageCallback callback) {
  auto send_message_closure = base::BindOnce(
      &SharingFCMSender::DoSendMessageToDevice, weak_ptr_factory_.GetWeakPtr(),
      std::move(target), time_to_live, std::move(message), std::move(callback));

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
    syncer::DeviceInfo::SharingInfo target,
    base::TimeDelta time_to_live,
    SharingMessage message,
    SendMessageCallback callback) {
  base::Optional<SharingSyncPreference::FCMRegistration> fcm_registration =
      sync_preference_->GetFCMRegistration();
  base::Optional<syncer::DeviceInfo::SharingInfo> sharing_info =
      sync_preference_->GetLocalSharingInfo();
  if (!fcm_registration || !sharing_info) {
    LOG(ERROR) << "Unable to retrieve FCM registration or sharing info";
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            base::nullopt);
    return;
  }

  const syncer::DeviceInfo* local_device_info =
      device_info_provider_->GetLocalDeviceInfo();
  message.set_sender_guid(local_device_info->guid());

  if (message.payload_case() != SharingMessage::kAckMessage) {
    message.set_sender_device_name(local_device_info->client_name());
    auto* sender_info = message.mutable_sender_info();
    sender_info->set_fcm_token(sharing_info->fcm_token);
    sender_info->set_p256dh(sharing_info->p256dh);
    sender_info->set_auth_secret(sharing_info->auth_secret);
  }

  gcm::WebPushMessage web_push_message;
  web_push_message.time_to_live = time_to_live.InSeconds();
  web_push_message.urgency = gcm::WebPushMessage::Urgency::kHigh;
  message.SerializeToString(&web_push_message.payload);

  gcm_driver_->SendWebPushMessage(
      kSharingFCMAppID, fcm_registration->authorized_entity, target.p256dh,
      target.auth_secret, target.fcm_token,
      vapid_key_manager_->GetOrCreateKey(), std::move(web_push_message),
      base::BindOnce(&SharingFCMSender::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SharingFCMSender::OnMessageSent(SendMessageCallback callback,
                                     gcm::SendWebPushMessageResult result,
                                     base::Optional<std::string> message_id) {
  SharingSendMessageResult send_message_result;
  switch (result) {
    case gcm::SendWebPushMessageResult::kSuccessful:
      send_message_result = SharingSendMessageResult::kSuccessful;
      break;
    case gcm::SendWebPushMessageResult::kDeviceGone:
      send_message_result = SharingSendMessageResult::kDeviceNotFound;
      break;
    case gcm::SendWebPushMessageResult::kNetworkError:
      send_message_result = SharingSendMessageResult::kNetworkError;
      break;
    case gcm::SendWebPushMessageResult::kPayloadTooLarge:
      send_message_result = SharingSendMessageResult::kPayloadTooLarge;
      break;
    case gcm::SendWebPushMessageResult::kEncryptionFailed:
    case gcm::SendWebPushMessageResult::kCreateJWTFailed:
    case gcm::SendWebPushMessageResult::kServerError:
    case gcm::SendWebPushMessageResult::kParseResponseFailed:
    case gcm::SendWebPushMessageResult::kVapidKeyInvalid:
      send_message_result = SharingSendMessageResult::kInternalError;
      break;
  }

  std::move(callback).Run(send_message_result, message_id);
}
