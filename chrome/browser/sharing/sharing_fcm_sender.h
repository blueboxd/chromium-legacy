// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_
#define CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "chrome/browser/sharing/web_push/web_push_sender.h"
#include "components/sync_device_info/device_info.h"

namespace gcm {
class GCMDriver;
enum class GCMEncryptionResult;
}  // namespace gcm

namespace syncer {
class LocalDeviceInfoProvider;
}  // namespace syncer

enum class SendWebPushMessageResult;
class SharingSyncPreference;
class VapidKeyManager;

// Responsible for sending FCM messages within Sharing infrastructure.
class SharingFCMSender : public SharingMessageSender::SendMessageDelegate {
 public:
  using SharingMessage = chrome_browser_sharing::SharingMessage;
  using SendMessageCallback =
      base::OnceCallback<void(SharingSendMessageResult,
                              base::Optional<std::string>)>;

  SharingFCMSender(std::unique_ptr<WebPushSender> web_push_sender,
                   SharingSyncPreference* sync_preference,
                   VapidKeyManager* vapid_key_manager,
                   gcm::GCMDriver* gcm_driver,
                   syncer::LocalDeviceInfoProvider* local_device_info_provider);
  SharingFCMSender(const SharingFCMSender&) = delete;
  SharingFCMSender& operator=(const SharingFCMSender&) = delete;
  ~SharingFCMSender() override;

  // Sends a |message| to device identified by |target|, which expires
  // after |time_to_live| seconds. |callback| will be invoked with message_id if
  // asynchronous operation succeeded, or base::nullopt if operation failed.
  virtual void SendMessageToTargetInfo(
      syncer::DeviceInfo::SharingTargetInfo target,
      base::TimeDelta time_to_live,
      SharingMessage message,
      SendMessageCallback callback);

  // Used to inject fake WebPushSender in integration tests.
  void SetWebPushSenderForTesting(
      std::unique_ptr<WebPushSender> web_push_sender);

 protected:
  // SharingMessageSender::SendMessageDelegate:
  void DoSendMessageToDevice(const syncer::DeviceInfo& device,
                             base::TimeDelta time_to_live,
                             SharingMessage message,
                             SendMessageCallback callback) override;

 private:
  void OnMessageEncrypted(std::string fcm_token,
                          base::TimeDelta time_to_live,
                          SendMessageCallback callback,
                          gcm::GCMEncryptionResult result,
                          std::string message);

  base::Optional<syncer::DeviceInfo::SharingTargetInfo> GetTargetInfo(
      const syncer::DeviceInfo& device);

  bool SetMessageSenderInfo(SharingMessage* message);

  void OnMessageSent(SendMessageCallback callback,
                     SendWebPushMessageResult result,
                     base::Optional<std::string> message_id);

  std::unique_ptr<WebPushSender> web_push_sender_;
  SharingSyncPreference* sync_preference_;
  VapidKeyManager* vapid_key_manager_;
  gcm::GCMDriver* gcm_driver_;
  syncer::LocalDeviceInfoProvider* local_device_info_provider_;

  base::WeakPtrFactory<SharingFCMSender> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_
