// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_
#define CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace gcm {
class GCMDriver;
}  // namespace gcm

class VapidKeyManager;

// Responsible for sending FCM messages within Sharing infrastructure.
class SharingFCMSender {
 public:
  using SendMessageCallback =
      base::OnceCallback<void(base::Optional<std::string>)>;

  SharingFCMSender(gcm::GCMDriver* gcm_driver,
                   syncer::LocalDeviceInfoProvider* device_info_provider,
                   SharingSyncPreference* sync_preference,
                   VapidKeyManager* vapid_key_manager);
  virtual ~SharingFCMSender();

  // Sends a |message| to device identified by |device_guid|, which expires
  // after |time_to_live| seconds. |callback| will be invoked with message_id if
  // asynchronous operation succeeded, or base::nullopt if operation failed.
  virtual void SendMessageToDevice(
      const std::string& device_guid,
      base::TimeDelta time_to_live,
      chrome_browser_sharing::SharingMessage message,
      SendMessageCallback callback);

 private:
  // Called once after the local device info is ready. This will only be used if
  // we tried to send a message before the local device info was ready.
  void OnLocalDeviceInfoInitialized();

  // Actually sends the |message| to |target|. This will be called when our
  // local device info is ready.
  void DoSendMessageToDevice(SharingSyncPreference::Device target,
                             base::TimeDelta time_to_live,
                             chrome_browser_sharing::SharingMessage message,
                             SendMessageCallback callback);

  gcm::GCMDriver* gcm_driver_;
  syncer::LocalDeviceInfoProvider* device_info_provider_;
  SharingSyncPreference* sync_preference_;
  VapidKeyManager* vapid_key_manager_;

  // Subscription for the case when the local device info is not ready yet.
  std::unique_ptr<syncer::LocalDeviceInfoProvider::Subscription>
      local_device_info_ready_subscription_;
  // Queued messages to be sent in OnLocalDeviceInfoInitialized.
  std::vector<base::OnceClosure> send_message_queue_;

  base::WeakPtrFactory<SharingFCMSender> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharingFCMSender);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_
