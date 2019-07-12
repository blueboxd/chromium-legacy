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
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "content/public/browser/browser_task_traits.h"

namespace {
// TODO(knollr): Should this be configurable or shared between similar features?
constexpr base::TimeDelta kDeviceExpiration = base::TimeDelta::FromDays(2);
constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay.  The interpretation of this value depends on
    // always_use_initial_delay.  It's either how long we wait between
    // requests before backoff starts, or how much we delay the first request
    // after backoff starts.
    5 * 60 * 1000,

    // Factor by which the waiting time will be multiplied.
    2.0,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.1,

    // Maximum amount of time we are willing to delay our request, -1
    // for no maximum.
    -1,

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // If true, we always use a delay of initial_delay_ms, even before
    // we've seen num_errors_to_ignore errors.  Otherwise, initial_delay_ms
    // is the first delay once we start exponential backoff.
    false,
};

}  // namespace

SharingService::SharingService(
    std::unique_ptr<SharingSyncPreference> sync_prefs,
    std::unique_ptr<VapidKeyManager> vapid_key_manager,
    std::unique_ptr<SharingDeviceRegistration> sharing_device_registration,
    std::unique_ptr<SharingFCMSender> fcm_sender,
    std::unique_ptr<SharingFCMHandler> fcm_handler,
    syncer::DeviceInfoTracker* device_info_tracker,
    syncer::SyncService* sync_service)
    : sync_prefs_(std::move(sync_prefs)),
      vapid_key_manager_(std::move(vapid_key_manager)),
      sharing_device_registration_(std::move(sharing_device_registration)),
      fcm_sender_(std::move(fcm_sender)),
      fcm_handler_(std::move(fcm_handler)),
      device_info_tracker_(device_info_tracker),
      sync_service_(sync_service),
      backoff_entry_(&kRetryBackoffPolicy),
      weak_ptr_factory_(this) {
  fcm_handler_->AddSharingHandler(
      chrome_browser_sharing::SharingMessage::kAckMessage,
      &ack_message_handler_);
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

  if (!sync_service_->HasObserver(this))
    sync_service_->AddObserver(this);
}

SharingService::~SharingService() {
  if (sync_service_ && sync_service_->HasObserver(this))
    sync_service_->RemoveObserver(this);
}

std::vector<SharingDeviceInfo> SharingService::GetDeviceCandidates(
    int required_capabilities) const {
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
  for (const auto& device : all_devices) {
    // If the current device is considered expired for our purposes, stop here
    // since the next devices in the vector are at least as expired than this
    // one.
    if (device->last_updated_timestamp() < min_updated_time)
      break;

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
          device->guid(), device->client_name(), device->device_type(),
          device->last_updated_timestamp(), device_capabilities);
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
  fcm_sender_->SendMessageToDevice(device_guid, time_to_live,
                                   std::move(message), std::move(callback));
}

void SharingService::RegisterHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_type,
    SharingMessageHandler* handler) {}

void SharingService::OnSyncShutdown(syncer::SyncService* sync) {
  if (sync_service_ && sync_service_->HasObserver(this))
    sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

void SharingService::OnStateChanged(syncer::SyncService* sync) {
  if (IsEnabled()) {
    // Trigger registration first time feature is enabled.
    if (sync_service_->HasObserver(this))
      sync_service_->RemoveObserver(this);
    AttemptRegistration();
  }
}

void SharingService::AttemptRegistration() {
  sharing_device_registration_->RegisterDevice(base::BindOnce(
      &SharingService::OnDeviceRegistered, weak_ptr_factory_.GetWeakPtr()));
}

void SharingService::OnDeviceRegistered(
    SharingDeviceRegistration::Result result) {
  switch (result) {
    case SharingDeviceRegistration::Result::SUCCESS:
      backoff_entry_.InformOfRequest(true);
      if (!device_registered_) {
        device_registered_ = true;
        fcm_handler_->StartListening();

        // Listen for further VAPID key changes for re-registration.
        sync_prefs_->SetVapidKeyChangeObserver(
            base::BindRepeating(&SharingService::AttemptRegistration,
                                weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    case SharingDeviceRegistration::Result::TRANSIENT_ERROR:
      backoff_entry_.InformOfRequest(false);
      // Transient error - try again after a delay.
      LOG(ERROR) << "Device registration failed with transient error";
      base::PostDelayedTaskWithTraits(
          FROM_HERE,
          {base::TaskPriority::BEST_EFFORT, content::BrowserThread::UI},
          base::BindOnce(&SharingService::AttemptRegistration,
                         weak_ptr_factory_.GetWeakPtr()),
          backoff_entry_.GetTimeUntilRelease());
      break;
    case SharingDeviceRegistration::Result::FATAL_ERROR:
      backoff_entry_.InformOfRequest(false);
      // No need to bother retrying in the case of one of fatal errors.
      LOG(ERROR) << "Device registration failed with fatal error";
      break;
  }
}

bool SharingService::IsEnabled() const {
  bool sync_enabled_and_active =
      sync_service_ &&
      sync_service_->GetTransportState() ==
          syncer::SyncService::TransportState::ACTIVE &&
      sync_service_->GetActiveDataTypes().Has(syncer::PREFERENCES);
  bool device_registration_enabled =
      base::FeatureList::IsEnabled(kSharingDeviceRegistration);

  return sync_enabled_and_active && device_registration_enabled;
}
