// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_feature_status_getter.h"

#include <utility>

namespace chromeos {

namespace device_sync {

CryptAuthFeatureStatusGetter::CryptAuthFeatureStatusGetter() = default;

CryptAuthFeatureStatusGetter::~CryptAuthFeatureStatusGetter() = default;

void CryptAuthFeatureStatusGetter::GetFeatureStatuses(
    const cryptauthv2::RequestContext& request_context,
    const base::flat_set<std::string>& device_ids,
    GetFeatureStatusesAttemptFinishedCallback callback) {
  // Enforce that GetFeatureStatuses() can only be called once.
  DCHECK(!was_get_feature_statuses_called_);
  was_get_feature_statuses_called_ = true;

  callback_ = std::move(callback);

  OnAttemptStarted(request_context, device_ids);
}

void CryptAuthFeatureStatusGetter::OnAttemptFinished(
    const IdToFeatureStatusMap& id_to_feature_status_map,
    const CryptAuthDeviceSyncResult::ResultCode& device_sync_result_code) {
  DCHECK(callback_);
  std::move(callback_).Run(id_to_feature_status_map, device_sync_result_code);
}

}  // namespace device_sync

}  // namespace chromeos
