// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_SERVICE_H_
#define CHROME_BROWSER_SHARING_SHARING_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/sharing/ack_message_handler.h"
#include "chrome/browser/sharing/ping_message_handler.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "net/base/backoff_entry.h"

#if defined(OS_ANDROID)
#include "chrome/browser/sharing/click_to_call/click_to_call_message_handler_android.h"
#endif  // defined(OS_ANDROID)

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace syncer {
class DeviceInfoTracker;
class LocalDeviceInfoProvider;
class SyncService;
}  // namespace syncer

class SharingDeviceInfo;
class SharingFCMHandler;
class SharingFCMSender;
class SharingMessageHandler;
class SharingSyncPreference;
class VapidKeyManager;
enum class SharingDeviceRegistrationResult;

// Class to manage lifecycle of sharing feature, and provide APIs to send
// sharing messages to other devices.
class SharingService : public KeyedService,
                       syncer::SyncServiceObserver,
                       AckMessageHandler::AckMessageObserver {
 public:
  using SendMessageCallback = base::OnceCallback<void(bool)>;

  enum class State {
    // Device is unregistered with FCM and Sharing is unavailable.
    DISABLED,
    // Device registration is in progress.
    REGISTERING,
    // Device is fully registered with FCM and Sharing is available.
    ACTIVE,
    // Device un-registration is in progress.
    UNREGISTERING
  };

  SharingService(
      std::unique_ptr<SharingSyncPreference> sync_prefs,
      std::unique_ptr<VapidKeyManager> vapid_key_manager,
      std::unique_ptr<SharingDeviceRegistration> sharing_device_registration,
      std::unique_ptr<SharingFCMSender> fcm_sender,
      std::unique_ptr<SharingFCMHandler> fcm_handler,
      gcm::GCMDriver* gcm_driver,
      syncer::DeviceInfoTracker* device_info_tracker,
      syncer::LocalDeviceInfoProvider* local_device_info_provider,
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

  // Returns the current state of SharingService.
  virtual State GetState() const;

 private:
  // Overrides for syncer::SyncServiceObserver.
  void OnSyncShutdown(syncer::SyncService* sync) override;
  void OnStateChanged(syncer::SyncService* sync) override;

  // AckMessageHandler::AckMessageObserver override.
  void OnAckReceived(const std::string& message_id) override;

  void RegisterDevice();
  void UnregisterDevice();
  void OnDeviceRegistered(SharingDeviceRegistrationResult result);
  void OnDeviceUnregistered(SharingDeviceRegistrationResult result);
  void OnMessageSent(base::TimeTicks start_time,
                     const std::string& message_guid,
                     base::Optional<std::string> message_id);
  void InvokeSendMessageCallback(const std::string& message_guid, bool result);

  // Returns true if required sync feature is enabled.
  bool IsSyncEnabled() const;

  // Returns true if required sync feature is disabled. Returns false if sync is
  // in transitioning state.
  bool IsSyncDisabled() const;

  std::unique_ptr<SharingSyncPreference> sync_prefs_;
  std::unique_ptr<VapidKeyManager> vapid_key_manager_;
  std::unique_ptr<SharingDeviceRegistration> sharing_device_registration_;
  std::unique_ptr<SharingFCMSender> fcm_sender_;
  std::unique_ptr<SharingFCMHandler> fcm_handler_;
  syncer::DeviceInfoTracker* device_info_tracker_;
  syncer::LocalDeviceInfoProvider* local_device_info_provider_;
  syncer::SyncService* sync_service_;
  AckMessageHandler ack_message_handler_;
  PingMessageHandler ping_message_handler_;
  net::BackoffEntry backoff_entry_;
  State state_;

  // Map of random GUID to SendMessageCallback.
  std::map<std::string, SendMessageCallback> send_message_callbacks_;

  // Map of FCM message_id to time at start of send message request to FCM.
  std::map<std::string, base::TimeTicks> send_message_times_;

  // Map of FCM message_id to random GUID.
  std::map<std::string, std::string> message_guids_;

#if defined(OS_ANDROID)
  ClickToCallMessageHandler click_to_call_message_handler_;
#endif  // defined(OS_ANDROID)

  base::WeakPtrFactory<SharingService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharingService);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_SERVICE_H_
