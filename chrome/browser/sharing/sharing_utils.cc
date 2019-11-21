// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_utils.h"

#include "base/feature_list.h"
#include "chrome/browser/sharing/features.h"
#include "components/sync/driver/sync_service.h"

namespace {

syncer::ModelTypeSet GetRequiredSyncDataTypes() {
  // DeviceInfo is always required to list devices.
  syncer::ModelTypeSet required_data_types(syncer::DEVICE_INFO);

  // Legacy implementation of device list and VAPID key uses sync preferences.
  if (!base::FeatureList::IsEnabled(kSharingUseDeviceInfo) ||
      !base::FeatureList::IsEnabled(kSharingDeriveVapidKey)) {
    required_data_types.Put(syncer::PREFERENCES);
  }

  return required_data_types;
}

}  // namespace

bool IsSyncEnabledForSharing(syncer::SyncService* sync_service) {
  if (!sync_service)
    return false;

  bool is_sync_enabled =
      sync_service->GetTransportState() ==
          syncer::SyncService::TransportState::ACTIVE &&
      sync_service->GetActiveDataTypes().HasAll(GetRequiredSyncDataTypes());
  // TODO(crbug.com/1012226): Remove local sync check when we have dedicated
  // Sharing data type.
  if (base::FeatureList::IsEnabled(kSharingDeriveVapidKey))
    is_sync_enabled &= !sync_service->IsLocalSyncEnabled();
  return is_sync_enabled;
}

bool IsSyncDisabledForSharing(syncer::SyncService* sync_service) {
  if (!sync_service)
    return false;

  bool is_sync_disabled =
      sync_service->GetTransportState() ==
          syncer::SyncService::TransportState::DISABLED ||
      (sync_service->GetTransportState() ==
           syncer::SyncService::TransportState::ACTIVE &&
       !sync_service->GetActiveDataTypes().HasAll(GetRequiredSyncDataTypes()));
  // TODO(crbug.com/1012226): Remove local sync check when we have dedicated
  // Sharing data type.
  if (base::FeatureList::IsEnabled(kSharingDeriveVapidKey))
    is_sync_disabled |= sync_service->IsLocalSyncEnabled();
  return is_sync_disabled;
}
