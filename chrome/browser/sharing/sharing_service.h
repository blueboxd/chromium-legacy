// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_SERVICE_H_
#define CHROME_BROWSER_SHARING_SHARING_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/ack_message_handler.h"
#include "chrome/browser/sharing/ping_message_handler.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/driver/sync_service_observer.h"

namespace syncer {
class DeviceInfoTracker;
class SyncService;
}  // namespace syncer

class SharingDeviceInfo;
class SharingFCMHandler;
class SharingFCMSender;
class SharingMessageHandler;
class SharingSyncPreference;
class VapidKeyManager;

// Class to manage lifecycle of sharing feature, and provide APIs to send
// sharing messages to other devices.
class SharingService : public KeyedService, syncer::SyncServiceObserver {
 public:
  using SendMessageCallback =
      base::OnceCallback<void(base::Optional<std::string>)>;

  SharingService(
      std::unique_ptr<SharingSyncPreference> sync_prefs,
      std::unique_ptr<VapidKeyManager> vapid_key_manager,
      std::unique_ptr<SharingDeviceRegistration> sharing_device_registration,
      std::unique_ptr<SharingFCMSender> fcm_sender,
      std::unique_ptr<SharingFCMHandler> fcm_handler,
      syncer::DeviceInfoTracker* device_info_tracker,
      syncer::SyncService* sync_service);
  ~SharingService() override;

  // Returns a list of DeviceInfo that is available to receive messages.
  // All returned devices has the specified |required_capabilities| defined in
  // SharingDeviceCapability enum.
  virtual std::vector<SharingDeviceInfo> GetDeviceCandidates(
      int required_capabilities) const;

  // Sends a message to the device specified by GUID.
  // |callback| will be invoked with message_id if synchronous operation
  // succeeded, or base::nullopt if operation failed.
  virtual void SendMessageToDevice(
      const std::string& device_guid,
      base::TimeDelta time_to_live,
      chrome_browser_sharing::SharingMessage message,
      SendMessageCallback callback);

  // Registers a handler of a given SharingMessage payload type.
  void RegisterHandler(
      chrome_browser_sharing::SharingMessage::PayloadCase payload_type,
      SharingMessageHandler* handler);

 private:
  // Overrides for syncer::SyncServiceObserver.
  void OnSyncShutdown(syncer::SyncService* sync) override;
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

  void OnDeviceRegistered(SharingDeviceRegistration::Result result);

  // Returns true if cross-device Sharing features enabled, false otherwise.
  bool IsEnabled() const;

  std::unique_ptr<SharingSyncPreference> sync_prefs_;
  std::unique_ptr<VapidKeyManager> vapid_key_manager_;
  std::unique_ptr<SharingDeviceRegistration> sharing_device_registration_;
  std::unique_ptr<SharingFCMSender> fcm_sender_;
  std::unique_ptr<SharingFCMHandler> fcm_handler_;
  syncer::DeviceInfoTracker* device_info_tracker_;
  syncer::SyncService* sync_service_;
  AckMessageHandler ack_message_handler_;
  PingMessageHandler ping_message_handler_;

  base::WeakPtrFactory<SharingService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharingService);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_SERVICE_H_
