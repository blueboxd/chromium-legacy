// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections.h"

namespace location {
namespace nearby {
namespace connections {

NearbyConnections::NearbyConnections(
    mojo::PendingReceiver<mojom::NearbyConnections> nearby_connections,
    mojo::PendingRemote<mojom::NearbyConnectionsHost> host,
    base::OnceClosure on_disconnect)
    : nearby_connections_(this, std::move(nearby_connections)),
      host_(std::move(host)),
      on_disconnect_(std::move(on_disconnect)) {
  nearby_connections_.set_disconnect_handler(base::BindOnce(
      &NearbyConnections::OnDisconnect, weak_ptr_factory_.GetWeakPtr()));
  host_.set_disconnect_handler(base::BindOnce(&NearbyConnections::OnDisconnect,
                                              weak_ptr_factory_.GetWeakPtr()));
  host_->GetBluetoothAdapter(
      base::BindOnce(&NearbyConnections::OnGetBluetoothAdapter,
                     weak_ptr_factory_.GetWeakPtr()));
}

NearbyConnections::~NearbyConnections() = default;

void NearbyConnections::OnDisconnect() {
  if (on_disconnect_)
    std::move(on_disconnect_).Run();
  // Note: |this| might be destroyed here.
}

void NearbyConnections::OnGetBluetoothAdapter(
    mojo::PendingRemote<::bluetooth::mojom::Adapter> pending_remote_adapter) {
  if (!pending_remote_adapter.is_valid()) {
    VLOG(1) << __func__
            << " Received invalid Bluetooh adapter in utility process";
    return;
  }

  VLOG(1) << __func__ << " Received Bluetooh adapter in utility process";

  bluetooth_adapter_.Bind(std::move(pending_remote_adapter));
  bluetooth_adapter_->GetInfo(
      base::BindOnce([](bluetooth::mojom::AdapterInfoPtr info) {
        VLOG(1) << __func__ << "Bluetooh AdapterInfo name: '" << info->name
                << "' system_name: '" << info->system_name << "' address: '"
                << info->address << "' present: " << info->present
                << " powered: " << info->powered;
      }));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
