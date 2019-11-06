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
#include "chrome/browser/sharing/sharing_utils.h"
#include "chrome/browser/sharing/sms/sms_fetch_request_handler.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "content/public/browser/browser_task_traits.h"

#if defined(OS_ANDROID)
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_android.h"
#include "chrome/browser/sharing/sharing_service_proxy_android.h"
#else
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_desktop.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
#include "chrome/browser/sharing/shared_clipboard/remote_copy_message_handler.h"
#endif

SharingService::SharingService(
    Profile* profile,
    std::unique_ptr<SharingSyncPreference> sync_prefs,
    std::unique_ptr<VapidKeyManager> vapid_key_manager,
    std::unique_ptr<SharingDeviceRegistration> sharing_device_registration,
    std::unique_ptr<SharingFCMSender> fcm_sender,
    std::unique_ptr<SharingFCMHandler> fcm_handler,
    std::unique_ptr<SharingMessageSender> message_sender,
    gcm::GCMDriver* gcm_driver,
    syncer::DeviceInfoTracker* device_info_tracker,
    syncer::LocalDeviceInfoProvider* local_device_info_provider,
    syncer::SyncService* sync_service)
    : sync_prefs_(std::move(sync_prefs)),
      vapid_key_manager_(std::move(vapid_key_manager)),
      sharing_device_registration_(std::move(sharing_device_registration)),
      fcm_sender_(std::move(fcm_sender)),
      fcm_handler_(std::move(fcm_handler)),
      message_sender_(std::move(message_sender)),
      device_info_tracker_(device_info_tracker),
      local_device_info_provider_(local_device_info_provider),
      sync_service_(sync_service),
      backoff_entry_(&kRetryBackoffPolicy),
      state_(State::DISABLED),
      is_observing_device_info_tracker_(false) {
  // Remove old encryption info with empty authorized_entity to avoid DCHECK.
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
      chrome_browser_sharing::SharingMessage::kPingMessage,
      &ping_message_handler_);

  ack_message_handler_ =
      std::make_unique<AckMessageHandler>(message_sender_.get());
  fcm_handler_->AddSharingHandler(
      chrome_browser_sharing::SharingMessage::kAckMessage,
      ack_message_handler_.get());

#if defined(OS_ANDROID)
  // Note: IsClickToCallSupported() is not used as it requires JNI call.
  if (base::FeatureList::IsEnabled(kClickToCallReceiver)) {
    fcm_handler_->AddSharingHandler(
        chrome_browser_sharing::SharingMessage::kClickToCallMessage,
        &click_to_call_message_handler_);
  }

  if (sharing_device_registration_->IsSmsFetcherSupported()) {
    sms_fetch_request_handler_ = std::make_unique<SmsFetchRequestHandler>();
    fcm_handler_->AddSharingHandler(
        chrome_browser_sharing::SharingMessage::kSmsFetchRequest,
        sms_fetch_request_handler_.get());
  }
#endif  // defined(OS_ANDROID)

  if (sharing_device_registration_->IsSharedClipboardSupported()) {
#if defined(OS_ANDROID)
    shared_clipboard_message_handler_ =
        std::make_unique<SharedClipboardMessageHandlerAndroid>(this);
#else
    shared_clipboard_message_handler_ =
        std::make_unique<SharedClipboardMessageHandlerDesktop>(this, profile);
#endif  // defined(OS_ANDROID)
    fcm_handler_->AddSharingHandler(
        chrome_browser_sharing::SharingMessage::kSharedClipboardMessage,
        shared_clipboard_message_handler_.get());
  }

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
  if (sharing_device_registration_->IsRemoteCopySupported()) {
    remote_copy_message_handler_ =
        std::make_unique<RemoteCopyMessageHandler>(profile);
    fcm_handler_->AddSharingHandler(
        chrome_browser_sharing::SharingMessage::kRemoteCopyMessage,
        remote_copy_message_handler_.get());
  }
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

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

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(syncer::GetPersonalizableDeviceNameBlocking),
      base::BindOnce(&SharingService::InitPersonalizableLocalDeviceName,
                     weak_ptr_factory_.GetWeakPtr()));
}

SharingService::~SharingService() {
  if (sync_service_ && sync_service_->HasObserver(this))
    sync_service_->RemoveObserver(this);
}

std::unique_ptr<syncer::DeviceInfo> SharingService::GetDeviceByGuid(
    const std::string& guid) const {
  if (!IsSyncEnabled())
    return nullptr;

  std::unique_ptr<syncer::DeviceInfo> device_info =
      device_info_tracker_->GetDeviceInfo(guid);
  device_info->set_client_name(
      GetSharingDeviceNames(device_info.get()).full_name);
  return device_info;
}

SharingService::SharingDeviceList SharingService::GetDeviceCandidates(
    sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const {
  if (IsSyncDisabled() || !local_device_info_provider_->GetLocalDeviceInfo() ||
      !personalizable_local_device_name_)
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
    base::TimeDelta response_timeout,
    chrome_browser_sharing::SharingMessage message,
    SharingMessageSender::ResponseCallback callback) {
  message_sender_->SendMessageToDevice(device_guid, response_timeout,
                                       std::move(message), std::move(callback));
}

void SharingService::SetDeviceInfoTrackerForTesting(
    syncer::DeviceInfoTracker* tracker) {
  device_info_tracker_ = tracker;
}

SharingService::State SharingService::GetStateForTesting() const {
  return state_;
}

SharingSyncPreference* SharingService::GetSyncPreferencesForTesting() const {
  return sync_prefs_.get();
}

SharingFCMHandler* SharingService::GetFCMHandlerForTesting() const {
  return fcm_handler_.get();
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

  std::unordered_map<std::string, SharingDeviceNames> device_candidate_names;
  std::unordered_set<std::string> full_device_names;
  std::unordered_map<std::string, int> short_device_name_counter;

  // To prevent adding candidates with same full name as local device.
  full_device_names.insert(
      GetSharingDeviceNames(local_device_info_provider_->GetLocalDeviceInfo())
          .full_name);
  // To prevent M78- instances of Chrome with same device model from showing up.
  full_device_names.insert(*personalizable_local_device_name_);

  for (const auto& device : devices) {
    SharingDeviceNames device_names = GetSharingDeviceNames(device.get());

    // Only insert the first occurrence of each device name.
    auto inserted = full_device_names.insert(device_names.full_name);
    if (!inserted.second)
      continue;

    short_device_name_counter[device_names.short_name]++;
    device_candidate_names.insert({device->guid(), std::move(device_names)});
  }

  // Rename filtered devices.
  SharingDeviceList device_candidates;
  for (auto& device : devices) {
    auto it = device_candidate_names.find(device->guid());
    if (it == device_candidate_names.end())
      continue;

    const SharingDeviceNames& device_names = it->second;
    bool is_short_name_unique =
        short_device_name_counter[device_names.short_name] == 1;

    device->set_client_name(is_short_name_unique ? device_names.short_name
                                                 : device_names.full_name);
    device_candidates.push_back(std::move(device));
  }

  return device_candidates;
}

void SharingService::InitPersonalizableLocalDeviceName(
    std::string personalizable_local_device_name) {
  personalizable_local_device_name_ =
      std::move(personalizable_local_device_name);
}
