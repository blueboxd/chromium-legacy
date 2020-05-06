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
}

NearbyConnections::~NearbyConnections() = default;

void NearbyConnections::OnDisconnect() {
  if (on_disconnect_)
    std::move(on_disconnect_).Run();
  // Note: |this| might be destroyed here.
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
