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
#include "components/sync_device_info/local_device_info_provider.h"

SharingFCMSender::SharingFCMSender(
    gcm::GCMDriver* gcm_driver,
    SharingSyncPreference* sync_preference,
    VapidKeyManager* vapid_key_manager,
    syncer::LocalDeviceInfoProvider* local_device_info_provider)
    : gcm_driver_(gcm_driver),
      sync_preference_(sync_preference),
      vapid_key_manager_(vapid_key_manager),
      local_device_info_provider_(local_device_info_provider) {}

SharingFCMSender::~SharingFCMSender() = default;

void SharingFCMSender::SendMessageToTargetInfo(
    syncer::DeviceInfo::SharingTargetInfo target,
    base::TimeDelta time_to_live,
    SharingMessage message,
    SendMessageCallback callback) {
  base::Optional<SharingSyncPreference::FCMRegistration> fcm_registration =
      sync_preference_->GetFCMRegistration();
  if (!fcm_registration) {
    LOG(ERROR) << "Unable to retrieve FCM registration";
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            base::nullopt);
    return;
  }

  auto* vapid_key = vapid_key_manager_->GetOrCreateKey();
  if (!vapid_key) {
    LOG(ERROR) << "Unable to retrieve VAPID key";
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            base::nullopt);
    return;
  }

  gcm::WebPushMessage web_push_message;
  web_push_message.time_to_live = time_to_live.InSeconds();
  web_push_message.urgency = gcm::WebPushMessage::Urgency::kHigh;
  message.SerializeToString(&web_push_message.payload);

  gcm_driver_->SendWebPushMessage(
      kSharingFCMAppID, fcm_registration->authorized_entity, target.p256dh,
      target.auth_secret, target.fcm_token, vapid_key,
      std::move(web_push_message),
      base::BindOnce(&SharingFCMSender::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SharingFCMSender::DoSendMessageToDevice(const syncer::DeviceInfo& device,
                                             base::TimeDelta time_to_live,
                                             SharingMessage message,
                                             SendMessageCallback callback) {
  base::Optional<syncer::DeviceInfo::SharingTargetInfo> target_info =
      GetTargetInfo(device);
  if (!target_info) {
    std::move(callback).Run(SharingSendMessageResult::kDeviceNotFound,
                            /*response=*/nullptr);
    return;
  }

  if (!SetMessageSenderInfo(&message)) {
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            /*response=*/nullptr);
    return;
  }

  SendMessageToTargetInfo(std::move(*target_info), time_to_live,
                          std::move(message), std::move(callback));
}

base::Optional<syncer::DeviceInfo::SharingTargetInfo>
SharingFCMSender::GetTargetInfo(const syncer::DeviceInfo& device) {
  if (device.sharing_info())
    return device.sharing_info()->vapid_target_info;

  // TODO(crbug/1015411): Here we assume caller gets the |device| from
  // GetDeviceCandidates, so DeviceInfoTracker is ready. It's better to queue up
  // the message and wait until DeviceInfoTracker is ready.
  return sync_preference_->GetTargetInfo(device.guid());
}

bool SharingFCMSender::SetMessageSenderInfo(SharingMessage* message) {
  base::Optional<syncer::DeviceInfo::SharingInfo> sharing_info =
      sync_preference_->GetLocalSharingInfo(
          local_device_info_provider_->GetLocalDeviceInfo());
  if (!sharing_info)
    return false;

  auto* fcm_configuration = message->mutable_fcm_channel_configuration();
  fcm_configuration->set_fcm_token(sharing_info->vapid_target_info.fcm_token);
  fcm_configuration->set_p256dh(sharing_info->vapid_target_info.p256dh);
  fcm_configuration->set_auth_secret(
      sharing_info->vapid_target_info.auth_secret);
  return true;
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
