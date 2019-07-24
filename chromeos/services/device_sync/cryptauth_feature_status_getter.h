// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_STATUS_GETTER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_STATUS_GETTER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"

namespace cryptauthv2 {
class RequestContext;
}  // namespace cryptauthv2

namespace chromeos {

namespace device_sync {

// Handles the BatchGetFeatureStatuses portion of the CryptAuth v2 DeviceSync
// protocol. Returns the feature statuses for each input device ID as a map from
// multidevice::SoftwareFeature to multidevice::SoftwareFeatureState.
//
// A CryptAuthFeatureStatusGetter object is designed to be used for only one
// GetFeatureStatuses() call. For a new attempt, a new object should be created.
class CryptAuthFeatureStatusGetter {
 public:
  using FeatureStatusMap =
      std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>;
  using IdToFeatureStatusMap = base::flat_map<std::string, FeatureStatusMap>;
  using GetFeatureStatusesAttemptFinishedCallback =
      base::OnceCallback<void(const IdToFeatureStatusMap&,
                              const CryptAuthDeviceSyncResult::ResultCode&)>;

  virtual ~CryptAuthFeatureStatusGetter();

  // Starts the BatchGetFeatureStatuses portion of the CryptAuth v2 DeviceSync
  // flow, retrieving feature status for |device_ids|.
  void GetFeatureStatuses(const cryptauthv2::RequestContext& request_context,
                          const base::flat_set<std::string>& device_ids,
                          GetFeatureStatusesAttemptFinishedCallback callback);

 protected:
  CryptAuthFeatureStatusGetter();

  // Implementations should retrieve feature statuses for devices with IDs
  // |device_ids|, using CryptAuth's BatchGetFeatureStatuses API, and call
  // OnAttemptFinished() with the results.
  virtual void OnAttemptStarted(
      const cryptauthv2::RequestContext& request_context,
      const base::flat_set<std::string>& device_ids) = 0;

  void OnAttemptFinished(
      const IdToFeatureStatusMap& id_to_feature_status_map,
      const CryptAuthDeviceSyncResult::ResultCode& device_sync_result_code);

 private:
  GetFeatureStatusesAttemptFinishedCallback callback_;
  bool was_get_feature_statuses_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthFeatureStatusGetter);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_STATUS_GETTER_H_
