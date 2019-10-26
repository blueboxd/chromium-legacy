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
#include "chrome/browser/sharing/click_to_call/click_to_call_message_handler_android.h"
#include "chrome/browser/sharing/sharing_service_proxy_android.h"
#endif  // defined(OS_ANDROID)

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace syncer {
class DeviceInfo;
class SyncService;
}  // namespace syncer

class NotificationDisplayService;
class SharedClipboardMessageHandler;
class SharingFCMHandler;
class SharingFCMSender;
class SharingSyncPreference;
class SmsFetchRequestHandler;
class VapidKeyManager;
enum class SharingDeviceRegistrationResult;

// Class to manage lifecycle of sharing feature, and provide APIs to send
// sharing messages to other devices.
class SharingService : public KeyedService,
                       syncer::SyncServiceObserver,
                       syncer::DeviceInfoTracker::Observer {
 public:
  using SendMessageCallback = base::OnceCallback<void(
      SharingSendMessageResult,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage>)>;
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

  // Sends a Sharing message to remote device.
  // |device_guid|: Sync GUID of receiver device.
  // |response_timeout|: Maximum amount of time waiting for a response before
  // invoking |callback| with kAckTimeout.
  // |message|: Message to be sent.
  // |callback| will be invoked once a response has received from remote device,
  // or if operation has failed or timed out.
  virtual void SendMessageToDevice(
      const std::string& device_guid,
      base::TimeDelta response_timeout,
      chrome_browser_sharing::SharingMessage message,
      SendMessageCallback callback);

  // Used to register devices with required capabilities in tests.
  void RegisterDeviceInTesting(
      std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_features,
      SharingDeviceRegistration::RegistrationCallback callback);

  // Used to fake client names in integration tests.
  void SetDeviceInfoTrackerForTesting(syncer::DeviceInfoTracker* tracker);

  // Returns the current state of SharingService for testing.
  State GetStateForTesting() const;

  // Returns SharingSyncPreference for integration tests.
  SharingSyncPreference* GetSyncPreferencesForTesting() const;

 private:
  // Overrides for syncer::SyncServiceObserver.
  void OnSyncShutdown(syncer::SyncService* sync) override;
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

  void OnAckReceived(
      chrome_browser_sharing::MessageType message_type,
      std::string message_id,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response);

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
      SharingSendMessageResult result,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response);

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

  void InitPersonalizableLocalDeviceName(
      std::string personalizable_local_device_name);

  std::unique_ptr<SharingSyncPreference> sync_prefs_;
  std::unique_ptr<VapidKeyManager> vapid_key_manager_;
  std::unique_ptr<SharingDeviceRegistration> sharing_device_registration_;
  std::unique_ptr<SharingFCMSender> fcm_sender_;
  std::unique_ptr<SharingFCMHandler> fcm_handler_;

  syncer::DeviceInfoTracker* device_info_tracker_;
  syncer::LocalDeviceInfoProvider* local_device_info_provider_;
  syncer::SyncService* sync_service_;

  net::BackoffEntry backoff_entry_;
  State state_;
  bool is_observing_device_info_tracker_;
  std::unique_ptr<syncer::LocalDeviceInfoProvider::Subscription>
      local_device_info_ready_subscription_;
  // The personalized name is stored for deduplicating devices running older
  // clients.
  base::Optional<std::string> personalizable_local_device_name_;

  // List of callbacks for AddDeviceCandidatesInitializedObserver.
  std::vector<base::OnceClosure> device_candidates_initialized_callbacks_;
  // Map of random GUID to SendMessageCallback.
  std::map<std::string, SendMessageCallback> send_message_callbacks_;
  // Map of FCM message_id to time at start of send message request to FCM.
  std::map<std::string, base::TimeTicks> send_message_times_;
  // Map of FCM message_id to random GUID.
  std::map<std::string, std::string> message_guids_;

#if defined(OS_ANDROID)
  SharingServiceProxyAndroid sharing_service_proxy_android_{this};
#endif  // defined(OS_ANDROID)

  PingMessageHandler ping_message_handler_;
  std::unique_ptr<AckMessageHandler> ack_message_handler_;
#if defined(OS_ANDROID)
  ClickToCallMessageHandler click_to_call_message_handler_;
  std::unique_ptr<SmsFetchRequestHandler> sms_fetch_request_handler_;
#endif  // defined(OS_ANDROID)
  std::unique_ptr<SharedClipboardMessageHandler>
      shared_clipboard_message_handler_;

  base::WeakPtrFactory<SharingService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharingService);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_SERVICE_H_
