// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_utils.h"

#include "base/feature_list.h"
#include "chrome/browser/sharing/features.h"
#include "components/sync/driver/sync_service.h"

namespace {

bool CanListDevices(syncer::SyncService* sync_service) {
  syncer::ModelTypeSet active_data_types = sync_service->GetActiveDataTypes();

  // Can list device using DeviceInfo and sharing.synced_devices preferences.
  if (active_data_types.HasAll({syncer::DEVICE_INFO, syncer::PREFERENCES}))
    return true;

  // Can list device using only DeviceInfo.
  if (active_data_types.Has(syncer::DEVICE_INFO)) {
    return true;
  }

  return false;
}

}  // namespace

bool CanSendViaVapid(syncer::SyncService* sync_service) {
  // Can send using VAPID key in sharing.vapid_key preferences.
  if (sync_service->GetActiveDataTypes().Has(syncer::PREFERENCES))
    return true;

  // TODO(crbug.com/1012226): Remove when derive VAPID key is removed.
  // Can send using derived VAPID key if local sync is disabled.
  return base::FeatureList::IsEnabled(kSharingDeriveVapidKey) &&
         !sync_service->IsLocalSyncEnabled();
}

bool CanSendViaSenderID(syncer::SyncService* sync_service) {
  return base::FeatureList::IsEnabled(kSharingSendViaSync) &&
         sync_service->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE);
}

bool IsSyncEnabledForSharing(syncer::SyncService* sync_service) {
  if (!sync_service)
    return false;

  if (sync_service->GetTransportState() !=
      syncer::SyncService::TransportState::ACTIVE) {
    return false;
  }

  if (!CanListDevices(sync_service)) {
    return false;
  }

  if (!CanSendViaVapid(sync_service) && !CanSendViaSenderID(sync_service)) {
    return false;
  }

  return true;
}

bool IsSyncDisabledForSharing(syncer::SyncService* sync_service) {
  // Sync service is not initialized, we can't be sure it's disabled.
  if (!sync_service)
    return false;

  if (sync_service->GetTransportState() ==
      syncer::SyncService::TransportState::DISABLED) {
    return true;
  }

  // Ignore transient states.
  if (sync_service->GetTransportState() !=
      syncer::SyncService::TransportState::ACTIVE) {
    return false;
  }

  if (!CanListDevices(sync_service))
    return true;

  if (!CanSendViaVapid(sync_service) && !CanSendViaSenderID(sync_service))
    return true;

  return false;
}
