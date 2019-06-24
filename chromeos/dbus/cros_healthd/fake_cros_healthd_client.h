// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_
#define CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"

namespace chromeos {

// Fake implementation of CrosHealthdClient.
class FakeCrosHealthdClient : public CrosHealthdClient {
 public:
  FakeCrosHealthdClient();
  explicit FakeCrosHealthdClient(
      mojo::PendingRemote<cros_healthd::mojom::CrosHealthdService>
          mock_service);
  ~FakeCrosHealthdClient() override;

  // CrosHealthdClient overrides:
  mojo::Remote<cros_healthd::mojom::CrosHealthdService> BootstrapMojoConnection(
      base::OnceCallback<void(bool success)> result_callback) override;

 private:
  mojo::PendingRemote<cros_healthd::mojom::CrosHealthdService> mock_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeCrosHealthdClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_
