// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service.h"

#include <map>

#include "base/time/time.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace {
// TODO(knollr): Should this be configurable or shared between similar features?
constexpr base::TimeDelta kDeviceExpiration = base::TimeDelta::FromDays(2);
}  // namespace

SharingService::SharingService(
    std::unique_ptr<SharingSyncPreference> sync_prefs,
    syncer::DeviceInfoTracker* device_info_tracker)
    : sync_prefs_(std::move(sync_prefs)),
      device_info_tracker_(device_info_tracker) {
  DCHECK(sync_prefs_);
  DCHECK(device_info_tracker_);
}

SharingService::~SharingService() = default;

std::vector<SharingDeviceInfo> SharingService::GetDeviceCandidates(
    int required_capabilities) const {
  std::vector<std::unique_ptr<syncer::DeviceInfo>> all_devices =
      device_info_tracker_->GetAllDeviceInfo();
  std::map<std::string, SharingSyncPreference::Device> synced_devices =
      sync_prefs_->GetSyncedDevices();

  const base::Time min_updated_time = base::Time::Now() - kDeviceExpiration;

  std::vector<SharingDeviceInfo> device_candidates;
  for (const auto& device : all_devices) {
    if (device->last_updated_timestamp() < min_updated_time)
      continue;

    auto synced_device = synced_devices.find(device->guid());
    if (synced_device == synced_devices.end())
      continue;

    int device_capabilities = synced_device->second.capabilities;
    if ((device_capabilities & required_capabilities) != required_capabilities)
      continue;

    device_candidates.emplace_back(device->guid(), device->client_name(),
                                   device->last_updated_timestamp(),
                                   device_capabilities);
  }

  // TODO(knollr): Remove devices from |sync_prefs_| that are in
  // |synced_devices| but not in |all_devices|?

  return device_candidates;
}

bool SharingService::SendMessageToDevice(
    const std::string& device_guid,
    const chrome_browser_sharing::SharingMessage& message) {
  return true;
}

void SharingService::RegisterHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_type,
    SharingMessageHandler* handler) {}
