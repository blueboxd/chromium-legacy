// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_CRYPTAUTH_INSTANCE_ID_PROVIDER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_CRYPTAUTH_INSTANCE_ID_PROVIDER_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chromeos/services/device_sync/public/cpp/cryptauth_instance_id_provider.h"

namespace chromeos {

namespace device_sync {

// Implementation of CryptAuthInstanceIdProvider for use in tests.
class FakeCryptAuthInstanceIdProvider : public CryptAuthInstanceIdProvider {
 public:
  FakeCryptAuthInstanceIdProvider();
  ~FakeCryptAuthInstanceIdProvider() override;

  // CryptAuthInstanceIdProvider:
  void GetInstanceId(GetInstanceIdCallback callback) override;
  void GetInstanceIdToken(CryptAuthService cryptauth_service,
                          GetInstanceIdTokenCallback callback) override;

  // Returns the array of callbacks sent to GetInstanceId(), ordered from first
  // call to last call. Because this array is returned by reference, the client
  // can invoke the callback of the i-th call using the following:
  //
  //     std::move(instance_id_callbacks()[i]).Run(...)
  std::vector<GetInstanceIdCallback>& instance_id_callbacks() {
    return instance_id_callbacks_;
  }

  // Returns the array of callbacks sent to GetInstanceIdToken() for the
  // given |cryptauth_service|, ordered from first call to last call. Because
  // this array is returned by reference, the client can invoke the callback of
  // the i-th call using the following:
  //
  //     std::move(instance_id_token_callbacks(<CryptAuthService>)[i]).Run(...)
  std::vector<GetInstanceIdTokenCallback>& instance_id_token_callbacks(
      const CryptAuthService& cryptauth_service) {
    return instance_id_token_callbacks_[cryptauth_service];
  }

 private:
  std::vector<GetInstanceIdCallback> instance_id_callbacks_;
  base::flat_map<CryptAuthService, std::vector<GetInstanceIdTokenCallback>>
      instance_id_token_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthInstanceIdProvider);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_CRYPTAUTH_INSTANCE_ID_PROVIDER_H_
