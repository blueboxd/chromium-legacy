// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_health/network_health_service.h"

#include "base/no_destructor.h"

namespace chromeos {
namespace network_health {

NetworkHealthService::NetworkHealthService() = default;

void NetworkHealthService::BindRemote(
    mojo::PendingReceiver<mojom::NetworkHealthService> receiver) {
  network_health_.BindRemote(std::move(receiver));
}

void NetworkHealthService::BindDiagnosticsRemote(
    mojo::PendingReceiver<
        network_diagnostics::mojom::NetworkDiagnosticsRoutines> receiver) {
  network_diagnostics_.BindReceiver(std::move(receiver));
}

NetworkHealthService* NetworkHealthService::GetInstance() {
  static base::NoDestructor<NetworkHealthService> instance;
  return instance.get();
}

}  // namespace network_health
}  // namespace chromeos
