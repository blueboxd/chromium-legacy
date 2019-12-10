// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_MESSAGE_SENDER_H_
#define CHROME_BROWSER_SHARING_SHARING_MESSAGE_SENDER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"

namespace chrome_browser_sharing {
enum MessageType : int;
class ResponseMessage;
class SharingMessage;
}  // namespace chrome_browser_sharing

namespace syncer {
class DeviceInfo;
class LocalDeviceInfoProvider;
}  // namespace syncer

class SharingFCMSender;
class SharingSyncPreference;
enum class SharingDevicePlatform;
enum class SharingSendMessageResult;

class SharingMessageSender {
 public:
  using ResponseCallback = base::OnceCallback<void(
      SharingSendMessageResult,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage>)>;

  SharingMessageSender(
      std::unique_ptr<SharingFCMSender> sharing_fcm_sender,
      SharingSyncPreference* sync_prefs,
      syncer::LocalDeviceInfoProvider* local_device_info_provider);
  virtual ~SharingMessageSender();

  virtual void SendMessageToDevice(
      const syncer::DeviceInfo& device,
      base::TimeDelta response_timeout,
      chrome_browser_sharing::SharingMessage message,
      ResponseCallback callback);

  virtual void OnAckReceived(
      chrome_browser_sharing::MessageType message_type,
      const std::string& message_id,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response);

 private:
  void OnMessageSent(base::TimeTicks start_time,
                     const std::string& message_guid,
                     chrome_browser_sharing::MessageType message_type,
                     SharingSendMessageResult result,
                     base::Optional<std::string> message_id);

  void InvokeSendMessageCallback(
      const std::string& message_guid,
      chrome_browser_sharing::MessageType message_type,
      SharingSendMessageResult result,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response);

  std::unique_ptr<SharingFCMSender> fcm_sender_;
  SharingSyncPreference* sync_prefs_;
  syncer::LocalDeviceInfoProvider* local_device_info_provider_;

  // Map of random GUID to SendMessageCallback.
  std::map<std::string, ResponseCallback> send_message_callbacks_;
  // Map of FCM message_id to time at start of send message request to FCM.
  std::map<std::string, base::TimeTicks> send_message_times_;
  // Map of FCM message_id to random GUID.
  std::map<std::string, std::string> message_guids_;
  // Map of random message guid to platform of receiver device for metrics.
  std::map<std::string, SharingDevicePlatform> receiver_device_platform_;

  base::WeakPtrFactory<SharingMessageSender> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharingMessageSender);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_MESSAGE_SENDER_H_
