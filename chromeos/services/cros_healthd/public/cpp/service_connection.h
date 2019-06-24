// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"

namespace chromeos {
namespace cros_healthd {

// Encapsulates a connection to the Chrome OS cros_healthd daemon via its Mojo
// interface.
// Sequencing: Must be used on a single sequence (may be created on another).
class ServiceConnection {
 public:
  static ServiceConnection* GetInstance();

  // Gather various info on non-removeable block devices.
  virtual void ProbeNonRemovableBlockDeviceInfo(
      mojom::CrosHealthdService::ProbeNonRemovableBlockDeviceInfoCallback
          callback) = 0;

 protected:
  ServiceConnection() = default;
  virtual ~ServiceConnection() = default;
};

}  // namespace cros_healthd
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_
