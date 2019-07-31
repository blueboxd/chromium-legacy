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
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "content/public/browser/browser_task_traits.h"

SharingService::SharingService(
    std::unique_ptr<SharingSyncPreference> sync_prefs,
    std::unique_ptr<VapidKeyManager> vapid_key_manager,
    std::unique_ptr<SharingDeviceRegistration> sharing_device_registration,
    std::unique_ptr<SharingFCMSender> fcm_sender,
    std::unique_ptr<SharingFCMHandler> fcm_handler,
    gcm::GCMDriver* gcm_driver,
    syncer::DeviceInfoTracker* device_info_tracker,
    syncer::LocalDeviceInfoProvider* local_device_info_provider,
    syncer::SyncService* sync_service)
    : sync_prefs_(std::move(sync_prefs)),
      vapid_key_manager_(std::move(vapid_key_manager)),
      sharing_device_registration_(std::move(sharing_device_registration)),
      fcm_sender_(std::move(fcm_sender)),
      fcm_handler_(std::move(fcm_handler)),
      device_info_tracker_(device_info_tracker),
      local_device_info_provider_(local_device_info_provider),
      sync_service_(sync_service),
      backoff_entry_(&kRetryBackoffPolicy),
      state_(State::DISABLED) {
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
        &click_to_call_message_handler_);
  }
#endif  // defined(OS_ANDROID)

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

std::vector<SharingDeviceInfo> SharingService::GetDeviceCandidates(
    int required_capabilities) const {
  if (!IsSyncEnabled())
    return std::vector<SharingDeviceInfo>();

  std::vector<std::unique_ptr<syncer::DeviceInfo>> all_devices =
      device_info_tracker_->GetAllDeviceInfo();
  std::map<std::string, SharingSyncPreference::Device> synced_devices =
      sync_prefs_->GetSyncedDevices();

  const base::Time min_updated_time = base::Time::Now() - kDeviceExpiration;

  // Sort the DeviceInfo vector so the most recently modified devices are first.
  std::sort(all_devices.begin(), all_devices.end(),
            [](const auto& device1, const auto& device2) {
              return device1->last_updated_timestamp() >
                     device2->last_updated_timestamp();
            });

  std::unordered_set<std::string> device_names;
  std::vector<SharingDeviceInfo> device_candidates;
  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  for (const auto& device : all_devices) {
    // If the current device is considered expired for our purposes, stop here
    // since the next devices in the vector are at least as expired than this
    // one.
    if (device->last_updated_timestamp() < min_updated_time)
      break;

    if (local_device_info &&
        (local_device_info->client_name() == device->client_name())) {
      continue;
    }

    auto synced_device = synced_devices.find(device->guid());
    if (synced_device == synced_devices.end())
      continue;

    int device_capabilities = synced_device->second.capabilities;
    if ((device_capabilities & required_capabilities) != required_capabilities)
      continue;

    // Only insert the first occurrence of each device name.
    auto inserted = device_names.insert(device->client_name());
    if (inserted.second) {
      device_candidates.emplace_back(
          device->guid(), base::UTF8ToUTF16(device->client_name()),
          device->device_type(), device->last_updated_timestamp(),
          device_capabilities);
    }
  }

  // TODO(knollr): Remove devices from |sync_prefs_| that are in
  // |synced_devices| but not in |all_devices|?

  return device_candidates;
}

void SharingService::SendMessageToDevice(
    const std::string& device_guid,
    base::TimeDelta time_to_live,
    chrome_browser_sharing::SharingMessage message,
    SendMessageCallback callback) {
  std::string message_guid = base::GenerateGUID();
  send_message_callbacks_.emplace(message_guid, std::move(callback));

  base::PostDelayedTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, content::BrowserThread::UI},
      base::BindOnce(&SharingService::InvokeSendMessageCallback,
                     weak_ptr_factory_.GetWeakPtr(), message_guid,
                     /*success=*/false),
      kSendMessageTimeout);

  fcm_sender_->SendMessageToDevice(
      device_guid, time_to_live, std::move(message),
      base::BindOnce(&SharingService::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     message_guid));
}

void SharingService::OnMessageSent(base::TimeTicks start_time,
                                   const std::string& message_guid,
                                   base::Optional<std::string> message_id) {
  if (!message_id) {
    InvokeSendMessageCallback(message_guid, /*success=*/false);
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
  InvokeSendMessageCallback(message_guid, /*success=*/true);
}

void SharingService::InvokeSendMessageCallback(const std::string& message_guid,
                                               bool result) {
  auto iter = send_message_callbacks_.find(message_guid);
  if (iter == send_message_callbacks_.end())
    return;

  SendMessageCallback callback = std::move(iter->second);
  send_message_callbacks_.erase(iter);
  std::move(callback).Run(result);
  LogSendSharingMessageSuccess(result);
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
      base::PostDelayedTaskWithTraits(
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
         sync_service_->GetActiveDataTypes().Has(syncer::PREFERENCES);
}

bool SharingService::IsSyncDisabled() const {
  return sync_service_ &&
         (sync_service_->GetTransportState() ==
              syncer::SyncService::TransportState::DISABLED ||
          (sync_service_->GetTransportState() ==
               syncer::SyncService::TransportState::ACTIVE &&
           !sync_service_->GetActiveDataTypes().Has(syncer::PREFERENCES)));
}
