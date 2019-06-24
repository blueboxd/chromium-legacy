// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"

#include "base/callback.h"

namespace chromeos {

FakeCrosHealthdClient::FakeCrosHealthdClient() = default;

FakeCrosHealthdClient::FakeCrosHealthdClient(
    mojo::PendingRemote<cros_healthd::mojom::CrosHealthdService> mock_service)
    : mock_service_(std::move(mock_service)) {}

FakeCrosHealthdClient::~FakeCrosHealthdClient() = default;

mojo::Remote<cros_healthd::mojom::CrosHealthdService>
FakeCrosHealthdClient::BootstrapMojoConnection(
    base::OnceCallback<void(bool success)> result_callback) {
  mojo::Remote<cros_healthd::mojom::CrosHealthdService> remote(
      std::move(mock_service_));

  std::move(result_callback).Run(/*success=*/true);
  return remote;
}

}  // namespace chromeos
