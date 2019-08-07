// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CRYPTAUTH_INSTANCE_ID_PROVIDER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CRYPTAUTH_INSTANCE_ID_PROVIDER_H_

#include <ostream>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"

namespace chromeos {

namespace device_sync {

// Provides the user with a unique and immutable GCM Instance ID for use across
// CryptAuth v2 services. Provides unique and immutable Instance ID tokens for
// each CryptAuth v2 service: Enrollment and DeviceSync.
class CryptAuthInstanceIdProvider {
 public:
  enum class CryptAuthService { kEnrollmentV2, kDeviceSyncV2 };

  CryptAuthInstanceIdProvider();
  virtual ~CryptAuthInstanceIdProvider();

  using GetInstanceIdCallback =
      base::OnceCallback<void(const base::Optional<std::string>&)>;
  using GetInstanceIdTokenCallback =
      base::OnceCallback<void(const base::Optional<std::string>&)>;

  // Provides the user with a unique and immutable GCM Instance ID for use
  // across CryptAuth v2 services; if the operation fails, null is passed to the
  // callback.
  virtual void GetInstanceId(GetInstanceIdCallback callback) = 0;

  // Provides the user with a unique and immutable GCM Instance ID token
  // corresponding to the CryptAuth service, |cryptauth_service|; if the
  // operation fails, null is passed to the callback.
  //
  // By retrieving a token for a given service, we implicitly authorize that
  // service to send us GCM push notifications; the token is the identifier of
  // that registration.
  virtual void GetInstanceIdToken(CryptAuthService cryptauth_service,
                                  GetInstanceIdTokenCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptAuthInstanceIdProvider);
};

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthInstanceIdProvider::CryptAuthService cryptauth_service);

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CRYPTAUTH_INSTANCE_ID_PROVIDER_H_
