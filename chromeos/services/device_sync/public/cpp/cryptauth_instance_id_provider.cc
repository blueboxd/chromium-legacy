// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/public/cpp/cryptauth_instance_id_provider.h"

namespace chromeos {

namespace device_sync {

CryptAuthInstanceIdProvider::CryptAuthInstanceIdProvider() = default;

CryptAuthInstanceIdProvider::~CryptAuthInstanceIdProvider() = default;

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthInstanceIdProvider::CryptAuthService cryptauth_service) {
  switch (cryptauth_service) {
    case CryptAuthInstanceIdProvider::CryptAuthService::kEnrollmentV2:
      stream << "[CryptAuth service: Enrollment v2]";
      break;
    case CryptAuthInstanceIdProvider::CryptAuthService::kDeviceSyncV2:
      stream << "[CryptAuth service: DeviceSync v2]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace chromeos
