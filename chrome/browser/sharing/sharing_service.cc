// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service.h"

#include <algorithm>
#include <map>
#include <memory>
#include <unordered_set>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "content/public/browser/browser_task_traits.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// Util function to return a string denoting the type of device.
std::string GetDeviceType(sync_pb::SyncEnums::DeviceType type) {
  int device_type_message_id = -1;

  switch (type) {
    case sync_pb::SyncEnums::TYPE_LINUX:
    case sync_pb::SyncEnums::TYPE_WIN:
    case sync_pb::SyncEnums::TYPE_CROS:
    case sync_pb::SyncEnums::TYPE_MAC:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_COMPUTER;
      break;

    case sync_pb::SyncEnums::TYPE_UNSET:
    case sync_pb::SyncEnums::TYPE_OTHER:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_DEVICE;
      break;

    case sync_pb::SyncEnums::TYPE_PHONE:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_PHONE;
      break;

    case sync_pb::SyncEnums::TYPE_TABLET:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_TABLET;
      break;
  }

  return l10n_util::GetStringUTF8(device_type_message_id);
}

struct DeviceNames {
  std::string full_name;
  std::string short_name;
};

// Returns full and short names for |device|.
DeviceNames GetDeviceNames(const syncer::DeviceInfo* device) {
  DCHECK(device);
  DeviceNames device_names;

  base::SysInfo::HardwareInfo hardware_info = device->hardware_info();

  // The model might be empty if other device is still on M78 or lower with sync
  // turned on.
  if (hardware_info.model.empty()) {
    device_names.full_name = device_names.short_name = device->client_name();
    return device_names;
  }

  sync_pb::SyncEnums::DeviceType type = device->device_type();

  // For chromeOS, return manufacturer + model.
  if (type == sync_pb::SyncEnums::TYPE_CROS) {
    device_names.short_name = device_names.full_name =
        base::StrCat({device->hardware_info().manufacturer, " ",
                      device->hardware_info().model});
    return device_names;
  }

  if (hardware_info.manufacturer == "Apple Inc.") {
    // Internal names of Apple devices are formatted as MacbookPro2,3 or
    // iPhone2,1 or Ipad4,1.
    device_names.short_name = hardware_info.model.substr(
        0, hardware_info.model.find_first_of("0123456789,"));
    device_names.full_name = hardware_info.model;
    return device_names;
  }

  device_names.short_name =
      base::StrCat({hardware_info.manufacturer, " ", GetDeviceType(type)});
  device_names.full_name =
      base::StrCat({device_names.short_name, " ", hardware_info.model});
  return device_names;
}

// Clones device with new device name.
std::unique_ptr<syncer::DeviceInfo> CloneDevice(
    const syncer::DeviceInfo* device,
    const std::string& device_name) {
  return std::make_unique<syncer::DeviceInfo>(
      device->guid(), device_name, device->chrome_version(),
      device->sync_user_agent(), device->device_type(),
      device->signin_scoped_device_id(), device->hardware_info(),
      device->last_updated_timestamp(),
      device->send_tab_to_self_receiving_enabled(), device->sharing_info());
}
}  // namespace

SharingService::SharingService(
    std::unique_ptr<SharingSyncPreference> sync_prefs,
    std::unique_ptr<VapidKeyManager> vapid_key_manager,
    std::unique_ptr<SharingDeviceRegistration> sharing_device_registration,
    std::unique_ptr<SharingFCMSender> fcm_sender,
    std::unique_ptr<SharingFCMHandler> fcm_handler,
    gcm::GCMDriver* gcm_driver,
    syncer::DeviceInfoTracker* device_info_tracker,
    syncer::LocalDeviceInfoProvider* local_device_info_provider,
    syncer::SyncService* sync_service,
    NotificationDisplayService* notification_display_service)
    : sync_prefs_(std::move(sync_prefs)),
      vapid_key_manager_(std::move(vapid_key_manager)),
      sharing_device_registration_(std::move(sharing_device_registration)),
      fcm_sender_(std::move(fcm_sender)),
      fcm_handler_(std::move(fcm_handler)),
      device_info_tracker_(device_info_tracker),
      local_device_info_provider_(local_device_info_provider),
      sync_service_(sync_service),
      backoff_entry_(&kRetryBackoffPolicy),
      state_(State::DISABLED),
      is_observing_device_info_tracker_(false) {
  // Remove old encryption info with empty authrozed_entity to avoid DCHECK.
  // See http://crbug/987591
  if (gcm_driver) {
    gcm::GCMEncryptionProvider* encryption_provider =
        gcm_driver->GetEncryptionProviderInternal();
    if (encryption_provider) {
      encryption_provider->RemoveEncryptionInfo(
          kSharingFCMAppID, /*authorized_entity=*/std::string(),
          base::DoNothing());
    }
  }

  // Initialize sharing handlers.
  fcm_handler_->AddSharingHandler(
      chrome_browser_sharing::SharingMessage::kAckMessage,
      &ack_message_handler_);
  ack_message_handler_.AddObserver(this);
  fcm_handler_->AddSharingHandler(
      chrome_browser_sharing::SharingMessage::kPingMessage,
      &ping_message_handler_);
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(kClickToCallReceiver)) {
    fcm_handler_->AddSharingHandler(
        chrome_browser_sharing::SharingMessage::kClickToCallMessage,
        sharing_service_proxy_android_.click_to_call_message_handler());
  }

  shared_clipboard_message_handler_ =
      std::make_unique<SharedClipboardMessageHandlerAndroid>(this);
#else
  shared_clipboard_message_handler_ =
      std::make_unique<SharedClipboardMessageHandlerDesktop>(
          this, notification_display_service);
#endif  // defined(OS_ANDROID)

  if (sharing_device_registration_->IsSharedClipboardSupported()) {
    fcm_handler_->AddSharingHandler(
        chrome_browser_sharing::SharingMessage::kSharedClipboardMessage,
        shared_clipboard_message_handler_.get());
  }

  // If device has already registered before, start listening to FCM right away
  // to avoid missing messages.
  if (sync_prefs_ && sync_prefs_->GetFCMRegistration())
    fcm_handler_->StartListening();

  if (sync_service_ && !sync_service_->HasObserver(this))
    sync_service_->AddObserver(this);

  // Only unregister if sync is disabled (not initializing).
  if (IsSyncDisabled()) {
    // state_ is kept as State::DISABLED as SharingService has never registered,
    // and only doing clean up via UnregisterDevice().
    UnregisterDevice();
  }
}

SharingService::~SharingService() {
  ack_message_handler_.RemoveObserver(this);

  if (sync_service_ && sync_service_->HasObserver(this))
    sync_service_->RemoveObserver(this);
}

std::unique_ptr<syncer::DeviceInfo> SharingService::GetDeviceByGuid(
    const std::string& guid) const {
  if (!IsSyncEnabled())
    return nullptr;

  std::unique_ptr<syncer::DeviceInfo> device_info =
      device_info_tracker_->GetDeviceInfo(guid);
  return CloneDevice(device_info.get(),
                     GetDeviceNames(device_info.get()).full_name);
}

SharingService::SharingDeviceList SharingService::GetDeviceCandidates(
    sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const {
  if (IsSyncDisabled() || !local_device_info_provider_->GetLocalDeviceInfo())
    return {};

  SharingDeviceList device_candidates =
      device_info_tracker_->GetAllDeviceInfo();
  device_candidates =
      FilterDeviceCandidates(std::move(device_candidates), required_feature);
  return RenameAndDeduplicateDevices(std::move(device_candidates));
}

void SharingService::AddDeviceCandidatesInitializedObserver(
    base::OnceClosure callback) {
  if (IsSyncDisabled()) {
    std::move(callback).Run();
    return;
  }

  bool is_device_info_tracker_ready = device_info_tracker_->IsSyncing();
  bool is_local_device_info_ready =
      local_device_info_provider_->GetLocalDeviceInfo();
  if (is_device_info_tracker_ready && is_local_device_info_ready) {
    std::move(callback).Run();
    return;
  }

  device_candidates_initialized_callbacks_.emplace_back(std::move(callback));

  if (!is_device_info_tracker_ready && !is_observing_device_info_tracker_) {
    device_info_tracker_->AddObserver(this);
    is_observing_device_info_tracker_ = true;
  }

  if (!is_local_device_info_ready && !local_device_info_ready_subscription_) {
    local_device_info_ready_subscription_ =
        local_device_info_provider_->RegisterOnInitializedCallback(
            base::BindRepeating(&SharingService::OnDeviceInfoChange,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

void SharingService::SendMessageToDevice(
    const std::string& device_guid,
    base::TimeDelta time_to_live,
    chrome_browser_sharing::SharingMessage message,
    SendMessageCallback callback) {
  std::string message_guid = base::GenerateGUID();
  send_message_callbacks_.emplace(message_guid, std::move(callback));
  chrome_browser_sharing::MessageType message_type =
      SharingPayloadCaseToMessageType(message.payload_case());

  base::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, content::BrowserThread::UI},
      base::BindOnce(&SharingService::InvokeSendMessageCallback,
                     weak_ptr_factory_.GetWeakPtr(), message_guid, message_type,
                     SharingSendMessageResult::kAckTimeout),
      kSendMessageTimeout);

  // TODO(crbug/1015411): Here we assume caller gets |device_guid| from
  // GetDeviceCandidates, so both DeviceInfoTracker and LocalDeviceInfoProvider
  // are already ready. It's better to queue up the message and wait until
  // DeviceInfoTracker and LocalDeviceInfoProvider are ready.
  base::Optional<syncer::DeviceInfo::SharingInfo> target_sharing_info =
      sync_prefs_->GetSharingInfo(device_guid);
  if (!target_sharing_info) {
    InvokeSendMessageCallback(message_guid, message_type,
                              SharingSendMessageResult::kDeviceNotFound);
    return;
  }

  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  if (!local_device_info) {
    InvokeSendMessageCallback(message_guid, message_type,
                              SharingSendMessageResult::kInternalError);
    return;
  }

  std::unique_ptr<syncer::DeviceInfo> sender_device_info = CloneDevice(
      local_device_info, GetDeviceNames(local_device_info).full_name);
  sender_device_info->set_sharing_info(
      sync_prefs_->GetLocalSharingInfo(local_device_info));

  if (!sender_device_info->sharing_info()) {
    InvokeSendMessageCallback(message_guid, message_type,
                              SharingSendMessageResult::kInternalError);
    return;
  }

  fcm_sender_->SendMessageToDevice(
      std::move(*target_sharing_info), time_to_live, std::move(message),
      std::move(sender_device_info),
      base::BindOnce(&SharingService::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     message_guid, message_type));
}

void SharingService::SetDeviceInfoTrackerForTesting(
    syncer::DeviceInfoTracker* tracker) {
  device_info_tracker_ = tracker;
}

void SharingService::OnMessageSent(
    base::TimeTicks start_time,
    const std::string& message_guid,
    chrome_browser_sharing::MessageType message_type,
    SharingSendMessageResult result,
    base::Optional<std::string> message_id) {
  if (result != SharingSendMessageResult::kSuccessful) {
    InvokeSendMessageCallback(message_guid, message_type, result);
    return;
  }

  send_message_times_.emplace(*message_id, start_time);
  message_guids_.emplace(*message_id, message_guid);
}

void SharingService::OnAckReceived(
    chrome_browser_sharing::MessageType message_type,
    const std::string& message_id) {
  auto times_iter = send_message_times_.find(message_id);
  if (times_iter != send_message_times_.end()) {
    LogSharingMessageAckTime(message_type,
                             base::TimeTicks::Now() - times_iter->second);
    send_message_times_.erase(times_iter);
  }

  auto iter = message_guids_.find(message_id);
  if (iter == message_guids_.end())
    return;

  std::string message_guid = std::move(iter->second);
  message_guids_.erase(iter);
  InvokeSendMessageCallback(message_guid, message_type,
                            SharingSendMessageResult::kSuccessful);
}

void SharingService::InvokeSendMessageCallback(
    const std::string& message_guid,
    chrome_browser_sharing::MessageType message_type,
    SharingSendMessageResult result) {
  auto iter = send_message_callbacks_.find(message_guid);
  if (iter == send_message_callbacks_.end())
    return;

  SendMessageCallback callback = std::move(iter->second);
  send_message_callbacks_.erase(iter);
  std::move(callback).Run(result);
  LogSendSharingMessageResult(message_type, result);
}

void SharingService::OnDeviceInfoChange() {
  if (!device_info_tracker_->IsSyncing() ||
      !local_device_info_provider_->GetLocalDeviceInfo()) {
    return;
  }

  device_info_tracker_->RemoveObserver(this);
  is_observing_device_info_tracker_ = false;
  local_device_info_ready_subscription_.reset();

  for (base::OnceClosure& callback : device_candidates_initialized_callbacks_) {
    std::move(callback).Run();
  }
  device_candidates_initialized_callbacks_.clear();
}

void SharingService::RegisterHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_type,
    SharingMessageHandler* handler) {}

SharingService::State SharingService::GetState() const {
  return state_;
}

void SharingService::OnSyncShutdown(syncer::SyncService* sync) {
  if (sync_service_ && sync_service_->HasObserver(this))
    sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

void SharingService::OnStateChanged(syncer::SyncService* sync) {
  if (IsSyncEnabled()) {
    if (base::FeatureList::IsEnabled(kSharingDeviceRegistration)) {
      if (state_ == State::DISABLED) {
        state_ = State::REGISTERING;
        RegisterDevice();
      }
    } else {
      // Unregister the device once and stop listening for sync state changes.
      // If feature is turned back on, Chrome needs to be restarted.
      if (sync_service_ && sync_service_->HasObserver(this))
        sync_service_->RemoveObserver(this);
      UnregisterDevice();
    }
  } else if (IsSyncDisabled() && state_ == State::ACTIVE) {
    state_ = State::UNREGISTERING;
    fcm_handler_->StopListening();
    sync_prefs_->ClearVapidKeyChangeObserver();
    UnregisterDevice();
  }
}

void SharingService::OnSyncCycleCompleted(syncer::SyncService* sync) {
  if (!base::FeatureList::IsEnabled(kSharingDeriveVapidKey) ||
      state_ != State::ACTIVE) {
    return;
  }

  RefreshVapidKey();
}

void SharingService::RefreshVapidKey() {
  if (vapid_key_manager_->RefreshCachedKey())
    RegisterDevice();
}

void SharingService::RegisterDevice() {
  sharing_device_registration_->RegisterDevice(base::BindOnce(
      &SharingService::OnDeviceRegistered, weak_ptr_factory_.GetWeakPtr()));
}

void SharingService::RegisterDeviceInTesting(
    std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_features,
    SharingDeviceRegistration::RegistrationCallback callback) {
  sharing_device_registration_->SetEnabledFeaturesForTesting(
      std::move(enabled_features));
  sharing_device_registration_->RegisterDevice(std::move(callback));
}

void SharingService::UnregisterDevice() {
  sharing_device_registration_->UnregisterDevice(base::BindOnce(
      &SharingService::OnDeviceUnregistered, weak_ptr_factory_.GetWeakPtr()));
}

void SharingService::OnDeviceRegistered(
    SharingDeviceRegistrationResult result) {
  LogSharingRegistrationResult(result);
  switch (result) {
    case SharingDeviceRegistrationResult::kSuccess:
      backoff_entry_.InformOfRequest(true);
      if (state_ == State::REGISTERING) {
        if (IsSyncEnabled()) {
          state_ = State::ACTIVE;
          fcm_handler_->StartListening();

          if (base::FeatureList::IsEnabled(kSharingDeriveVapidKey)) {
            // Refresh VAPID key in case it's changed during registration.
            RefreshVapidKey();
          } else {
            // Listen for further VAPID key changes for re-registration.
            // state_ is kept as State::ACTIVE during re-registration.
            sync_prefs_->SetVapidKeyChangeObserver(
                base::BindRepeating(&SharingService::RefreshVapidKey,
                                    weak_ptr_factory_.GetWeakPtr()));
          }
        } else if (IsSyncDisabled()) {
          // In case sync is disabled during registration, unregister it.
          state_ = State::UNREGISTERING;
          UnregisterDevice();
        }
      }
      // For registration as result of VAPID key change, state_ will be
      // State::ACTIVE, and we don't need to start listeners.
      break;
    case SharingDeviceRegistrationResult::kFcmTransientError:
    case SharingDeviceRegistrationResult::kSyncServiceError:
      backoff_entry_.InformOfRequest(false);
      // Transient error - try again after a delay.
      LOG(ERROR) << "Device registration failed with transient error";
      base::PostDelayedTask(
          FROM_HERE,
          {base::TaskPriority::BEST_EFFORT, content::BrowserThread::UI},
          base::BindOnce(&SharingService::RegisterDevice,
                         weak_ptr_factory_.GetWeakPtr()),
          backoff_entry_.GetTimeUntilRelease());
      break;
    case SharingDeviceRegistrationResult::kEncryptionError:
    case SharingDeviceRegistrationResult::kFcmFatalError:
      backoff_entry_.InformOfRequest(false);
      // No need to bother retrying in the case of one of fatal errors.
      LOG(ERROR) << "Device registration failed with fatal error";
      break;
    case SharingDeviceRegistrationResult::kDeviceNotRegistered:
      // Register device cannot return kDeviceNotRegistered.
      NOTREACHED();
  }
}

void SharingService::OnDeviceUnregistered(
    SharingDeviceRegistrationResult result) {
  LogSharingUnegistrationResult(result);
  if (IsSyncEnabled() &&
      base::FeatureList::IsEnabled(kSharingDeviceRegistration)) {
    // In case sync is enabled during un-registration, register it.
    state_ = State::REGISTERING;
    RegisterDevice();
  } else {
    state_ = State::DISABLED;
  }

  switch (result) {
    case SharingDeviceRegistrationResult::kSuccess:
      // Successfully unregistered, no-op
      break;
    case SharingDeviceRegistrationResult::kFcmTransientError:
    case SharingDeviceRegistrationResult::kSyncServiceError:
      LOG(ERROR) << "Device un-registration failed with transient error";
      break;
    case SharingDeviceRegistrationResult::kEncryptionError:
    case SharingDeviceRegistrationResult::kFcmFatalError:
      LOG(ERROR) << "Device un-registration failed with fatal error";
      break;
    case SharingDeviceRegistrationResult::kDeviceNotRegistered:
      // Device has not been registered, no-op.
      break;
  }
}

bool SharingService::IsSyncEnabled() const {
  return sync_service_ &&
         sync_service_->GetTransportState() ==
             syncer::SyncService::TransportState::ACTIVE &&
         sync_service_->GetActiveDataTypes().HasAll(GetRequiredSyncDataTypes());
}

SharingSyncPreference* SharingService::GetSyncPreferences() const {
  return sync_prefs_.get();
}

bool SharingService::IsSyncDisabled() const {
  return sync_service_ && (sync_service_->GetTransportState() ==
                               syncer::SyncService::TransportState::DISABLED ||
                           (sync_service_->GetTransportState() ==
                                syncer::SyncService::TransportState::ACTIVE &&
                            !sync_service_->GetActiveDataTypes().HasAll(
                                GetRequiredSyncDataTypes())));
}

syncer::ModelTypeSet SharingService::GetRequiredSyncDataTypes() const {
  // DeviceInfo is always required to list devices.
  syncer::ModelTypeSet required_data_types(syncer::DEVICE_INFO);

  // Legacy implementation of device list and VAPID key uses sync preferences.
  if (!base::FeatureList::IsEnabled(kSharingUseDeviceInfo) ||
      !base::FeatureList::IsEnabled(kSharingDeriveVapidKey)) {
    required_data_types.Put(syncer::PREFERENCES);
  }

  return required_data_types;
}

SharingService::SharingDeviceList SharingService::FilterDeviceCandidates(
    SharingDeviceList devices,
    sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const {
  const base::Time min_updated_time = base::Time::Now() - kDeviceExpiration;
  SharingDeviceList filtered_devices;

  for (auto& device : devices) {
    // Checks if |last_updated_timestamp| is not too old.
    if (device->last_updated_timestamp() < min_updated_time)
      continue;

    // Checks whether |device| supports |required_feature|.
    base::Optional<syncer::DeviceInfo::SharingInfo> sharing_info =
        sync_prefs_->GetSharingInfo(device.get());
    if (!sharing_info ||
        !sharing_info->enabled_features.count(required_feature)) {
      continue;
    }

    filtered_devices.push_back(std::move(device));
  }

  return filtered_devices;
}

SharingService::SharingDeviceList SharingService::RenameAndDeduplicateDevices(
    SharingDeviceList devices) const {
  // Sort the devices so the most recently modified devices are first.
  std::sort(devices.begin(), devices.end(),
            [](const auto& device1, const auto& device2) {
              return device1->last_updated_timestamp() >
                     device2->last_updated_timestamp();
            });

  std::unordered_map<std::string, DeviceNames> device_candidate_names;
  std::unordered_set<std::string> full_device_names;
  std::unordered_map<std::string, int> short_device_name_counter;

  // To prevent adding candidates with same full name as local device.
  full_device_names.insert(
      GetDeviceNames(local_device_info_provider_->GetLocalDeviceInfo())
          .full_name);

  for (const auto& device : devices) {
    DeviceNames device_names = GetDeviceNames(device.get());

    // Only insert the first occurrence of each device name.
    auto inserted = full_device_names.insert(device_names.full_name);
    if (!inserted.second)
      continue;

    short_device_name_counter[device_names.short_name]++;
    device_candidate_names.insert({device->guid(), std::move(device_names)});
  }

  // Rename filtered devices.
  SharingDeviceList device_candidates;
  for (const auto& device : devices) {
    auto it = device_candidate_names.find(device->guid());
    if (it == device_candidate_names.end())
      continue;

    const DeviceNames& device_names = it->second;
    bool is_short_name_unique =
        short_device_name_counter[device_names.short_name] == 1;
    device_candidates.push_back(CloneDevice(
        device.get(), is_short_name_unique ? device_names.short_name
                                           : device_names.full_name));
  }

  return device_candidates;
}
