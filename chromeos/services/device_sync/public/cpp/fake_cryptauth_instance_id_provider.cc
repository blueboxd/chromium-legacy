// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/public/cpp/fake_cryptauth_instance_id_provider.h"

namespace chromeos {

namespace device_sync {

FakeCryptAuthInstanceIdProvider::FakeCryptAuthInstanceIdProvider() = default;

FakeCryptAuthInstanceIdProvider::~FakeCryptAuthInstanceIdProvider() = default;

void FakeCryptAuthInstanceIdProvider::GetInstanceId(
    GetInstanceIdCallback callback) {
  instance_id_callbacks_.push_back(std::move(callback));
}

void FakeCryptAuthInstanceIdProvider::GetInstanceIdToken(
    CryptAuthService cryptauth_service,
    GetInstanceIdTokenCallback callback) {
  instance_id_token_callbacks_[cryptauth_service].push_back(
      std::move(callback));
}

}  // namespace device_sync

}  // namespace chromeos
