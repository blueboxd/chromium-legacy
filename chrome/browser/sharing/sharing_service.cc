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

// Util function to get name for a single device. |is_full_name| determines
// whether we should add the model name for the device.
std::string GetDeviceName(const syncer::DeviceInfo* device, bool is_full_name) {
  DCHECK(device);

  base::SysInfo::HardwareInfo hardware_info = device->hardware_info();

  // The model might be empty if other device is still on M78 or lower with sync
  // turned on.
  if (hardware_info.model.empty())
    return device->client_name();

  sync_pb::SyncEnums::DeviceType type = device->device_type();

  // For chromeOS, return manufacturer + model.
  if (type == sync_pb::SyncEnums::TYPE_CROS) {
    return base::StrCat({device->hardware_info().manufacturer, " ",
                         device->hardware_info().model});
  }

  if (hardware_info.manufacturer == "Apple Inc.") {
    if (is_full_name)
      return hardware_info.model;

    // Returns a more readable hardware model. Internal names of Apple devices
    // are formatted as MacbookPro2,3 or iPhone2,1 or Ipad4,1.
    return hardware_info.model.substr(
        0, hardware_info.model.find_first_of("0123456789,"));
  }

  std::string device_name =
      base::StrCat({hardware_info.manufacturer, " ", GetDeviceType(type)});
  if (is_full_name)
    base::StrAppend(&device_name, {" ", hardware_info.model});

  return device_name;
}

// Util function to rename devices for Sharing.
std::vector<std::unique_ptr<syncer::DeviceInfo>> RenameDevices(
    const std::vector<std::unique_ptr<syncer::DeviceInfo>>& devices) {
  std::map<std::string, int> similar_devices_counter;
  for (const auto& device : devices)
    similar_devices_counter[GetDeviceName(device.get(),
                                          /*is_full_name=*/false)]++;

  std::vector<std::unique_ptr<syncer::DeviceInfo>> renamed_devices;
  for (const auto& device : devices) {
    bool is_full_name_required = similar_devices_counter[GetDeviceName(
                                     device.get(), /*is_full_name=*/false)] > 1;
    std::string device_name =
        GetDeviceName(device.get(), is_full_name_required);

    renamed_devices.push_back(std::make_unique<syncer::DeviceInfo>(
        device->guid(), device_name, device->chrome_version(),
        device->sync_user_agent(), device->device_type(),
        device->signin_scoped_device_id(), device->hardware_info(),
        device->last_updated_timestamp(),
        device->send_tab_to_self_receiving_enabled(), device->sharing_info()));
  }
  return renamed_devices;
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

  return device_info_tracker_->GetDeviceInfo(guid);
}

// TODO(crbug.com/1014107) - Simplify this function. The function does a number
// of things and some calls are duplicated at the moment.
std::vector<std::unique_ptr<syncer::DeviceInfo>>
SharingService::GetDeviceCandidates(
    sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const {
  std::vector<std::unique_ptr<syncer::DeviceInfo>> device_candidates;
  std::vector<std::unique_ptr<syncer::DeviceInfo>> all_devices =
      device_info_tracker_->GetAllDeviceInfo();
  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();

  if (IsSyncDisabled() || all_devices.empty() || !local_device_info)
    return device_candidates;

  const base::Time min_updated_time = base::Time::Now() - kDeviceExpiration;

  // Sort the DeviceInfo vector so the most recently modified devices are first.
  std::sort(all_devices.begin(), all_devices.end(),
            [](const auto& device1, const auto& device2) {
              return device1->last_updated_timestamp() >
                     device2->last_updated_timestamp();
            });

  std::unordered_set<std::string> full_device_names;
  // To prevent adding candidates with same name as local device.
  full_device_names.insert(
      GetDeviceName(local_device_info, /*is_full_name=*/true));

  for (auto& device : all_devices) {
    // If the current device is considered expired for our purposes, stop here
    // since the next devices in the vector are at least as expired than this
    // one.
    if (device->last_updated_timestamp() < min_updated_time)
      break;

    base::Optional<syncer::DeviceInfo::SharingInfo> sharing_info =
        sync_prefs_->GetSharingInfo(device.get());
    if (!sharing_info ||
        !sharing_info->enabled_features.count(required_feature)) {
      continue;
    }

    // Only insert the first occurrence of each device name.
    auto inserted = full_device_names.insert(
        GetDeviceName(device.get(), /*is_full_name=*/true));
    if (inserted.second)
      device_candidates.push_back(std::move(device));
  }

  // TODO(knollr): Remove devices from |sync_prefs_| that are in
  // |synced_devices| but not in |all_devices|?

  device_candidates = RenameDevices(device_candidates);

  return device_candidates;
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

  base::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, content::BrowserThread::UI},
      base::BindOnce(&SharingService::InvokeSendMessageCallback,
                     weak_ptr_factory_.GetWeakPtr(), message_guid,
                     SharingSendMessageResult::kAckTimeout),
      kSendMessageTimeout);

  base::Optional<syncer::DeviceInfo::SharingInfo> sharing_info =
      sync_prefs_->GetSharingInfo(device_guid);
  if (!sharing_info) {
    InvokeSendMessageCallback(message_guid,
                              SharingSendMessageResult::kDeviceNotFound);
    return;
  }

  fcm_sender_->SendMessageToDevice(
      std::move(*sharing_info), time_to_live, std::move(message),
      base::BindOnce(&SharingService::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     message_guid));
}

void SharingService::SetDeviceInfoTrackerForTesting(
    syncer::DeviceInfoTracker* tracker) {
  device_info_tracker_ = tracker;
}

void SharingService::OnMessageSent(base::TimeTicks start_time,
                                   const std::string& message_guid,
                                   SharingSendMessageResult result,
                                   base::Optional<std::string> message_id) {
  if (result != SharingSendMessageResult::kSuccessful) {
    InvokeSendMessageCallback(message_guid, result);
    return;
  }

  send_message_times_.emplace(*message_id, start_time);
  message_guids_.emplace(*message_id, message_guid);
}

void SharingService::OnAckReceived(const std::string& message_id) {
  auto times_iter = send_message_times_.find(message_id);
  if (times_iter != send_message_times_.end()) {
    LogSharingMessageAckTime(base::TimeTicks::Now() - times_iter->second);
    send_message_times_.erase(times_iter);
  }

  auto iter = message_guids_.find(message_id);
  if (iter == message_guids_.end())
    return;

  std::string message_guid = std::move(iter->second);
  message_guids_.erase(iter);
  InvokeSendMessageCallback(message_guid,
                            SharingSendMessageResult::kSuccessful);
}

void SharingService::InvokeSendMessageCallback(
    const std::string& message_guid,
    SharingSendMessageResult result) {
  auto iter = send_message_callbacks_.find(message_guid);
  if (iter == send_message_callbacks_.end())
    return;

  SendMessageCallback callback = std::move(iter->second);
  send_message_callbacks_.erase(iter);
  std::move(callback).Run(result);
  LogSendSharingMessageResult(result);
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

void SharingService::RegisterDevice() {
  sharing_device_registration_->RegisterDevice(base::BindOnce(
      &SharingService::OnDeviceRegistered, weak_ptr_factory_.GetWeakPtr()));
}

void SharingService::RegisterDeviceInTesting(
    std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_feautres,
    SharingDeviceRegistration::RegistrationCallback callback) {
  sharing_device_registration_->SetEnabledFeaturesForTesting(
      std::move(enabled_feautres));
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

          // Listen for further VAPID key changes for re-registration.
          // state_ is kept as State::ACTIVE during re-registration.
          sync_prefs_->SetVapidKeyChangeObserver(base::BindRepeating(
              &SharingService::RegisterDevice, weak_ptr_factory_.GetWeakPtr()));
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
