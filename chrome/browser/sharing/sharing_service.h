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
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "components/gcm_driver/web_push_common.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "net/base/backoff_entry.h"

#if defined(OS_ANDROID)
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_android.h"
#include "chrome/browser/sharing/sharing_service_proxy_android.h"
#else
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_desktop.h"
#endif  // defined(OS_ANDROID)

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace syncer {
class DeviceInfo;
class SyncService;
}  // namespace syncer

class NotificationDisplayService;
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
                       AckMessageHandler::AckMessageObserver,
                       syncer::DeviceInfoTracker::Observer {
 public:
  using SendMessageCallback =
      base::OnceCallback<void(SharingSendMessageResult)>;
  using SharingDeviceList = std::vector<std::unique_ptr<syncer::DeviceInfo>>;

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
      syncer::SyncService* sync_service,
      NotificationDisplayService* notification_display_service);
  ~SharingService() override;

  // Returns the device matching |guid|, or nullptr if no match was found.
  virtual std::unique_ptr<syncer::DeviceInfo> GetDeviceByGuid(
      const std::string& guid) const;

  // Returns a list of DeviceInfo that is available to receive messages.
  // All returned devices have the specified |required_feature|.
  virtual SharingDeviceList GetDeviceCandidates(
      sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const;

  // Register |callback| so it will be invoked after all dependencies of
  // GetDeviceCandidates are ready.
  void AddDeviceCandidatesInitializedObserver(base::OnceClosure callback);

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

  // Used to register devices with required capabilities in tests.
  void RegisterDeviceInTesting(
      std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_features,
      SharingDeviceRegistration::RegistrationCallback callback);

  SharingSyncPreference* GetSyncPreferences() const;

  // Used to fake client names in integration tests.
  void SetDeviceInfoTrackerForTesting(syncer::DeviceInfoTracker* tracker);

 private:
  // Overrides for syncer::SyncServiceObserver.
  void OnSyncShutdown(syncer::SyncService* sync) override;
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

  // AckMessageHandler::AckMessageObserver override.
  void OnAckReceived(chrome_browser_sharing::MessageType message_type,
                     const std::string& message_id) override;

  // syncer::DeviceInfoTracker::Observer.
  void OnDeviceInfoChange() override;

  void RefreshVapidKey();

  void RegisterDevice();
  void UnregisterDevice();

  void OnDeviceRegistered(SharingDeviceRegistrationResult result);
  void OnDeviceUnregistered(SharingDeviceRegistrationResult result);

  void OnMessageSent(base::TimeTicks start_time,
                     const std::string& message_guid,
                     chrome_browser_sharing::MessageType message_type,
                     SharingSendMessageResult result,
                     base::Optional<std::string> message_id);
  void InvokeSendMessageCallback(
      const std::string& message_guid,
      chrome_browser_sharing::MessageType message_type,
      SharingSendMessageResult result);

  // Returns true if required sync feature is enabled.
  bool IsSyncEnabled() const;

  // Returns true if required sync feature is disabled. Returns false if sync is
  // in transitioning state.
  bool IsSyncDisabled() const;

  // Returns all required sync data types to enable Sharing feature.
  syncer::ModelTypeSet GetRequiredSyncDataTypes() const;

  // Returns list of devices that have |required_feature| enabled. Also
  // filters out devices which have not been online for more than
  // |SharingConstants::kDeviceExpiration| time.
  SharingDeviceList FilterDeviceCandidates(
      SharingDeviceList devices,
      sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const;

  // Deduplicates devices based on client name. For devices with duplicate
  // client names, only the most recently updated device is filtered in.
  // Returned devices are renamed using the RenameDevice function
  // and are sorted in (not strictly) descending order by
  // last_updated_timestamp.
  SharingDeviceList RenameAndDeduplicateDevices(
      SharingDeviceList devices) const;

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
  std::vector<base::OnceClosure> device_candidates_initialized_callbacks_;
  bool is_observing_device_info_tracker_;
  std::unique_ptr<syncer::LocalDeviceInfoProvider::Subscription>
      local_device_info_ready_subscription_;

  // Map of random GUID to SendMessageCallback.
  std::map<std::string, SendMessageCallback> send_message_callbacks_;

  // Map of FCM message_id to time at start of send message request to FCM.
  std::map<std::string, base::TimeTicks> send_message_times_;

  // Map of FCM message_id to random GUID.
  std::map<std::string, std::string> message_guids_;

#if defined(OS_ANDROID)
  SharingServiceProxyAndroid sharing_service_proxy_android_{this};
#endif  // defined(OS_ANDROID)

  std::unique_ptr<SharedClipboardMessageHandler>
      shared_clipboard_message_handler_;

  base::WeakPtrFactory<SharingService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharingService);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_SERVICE_H_
