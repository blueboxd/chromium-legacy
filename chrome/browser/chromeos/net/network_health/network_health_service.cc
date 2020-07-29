// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_health/network_health_service.h"

#include "base/no_destructor.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {
namespace network_health {

NetworkHealthService::NetworkHealthService() {
  network_diagnostics_ =
      std::make_unique<network_diagnostics::NetworkDiagnosticsImpl>(
          chromeos::DBusThreadManager::Get()->GetDebugDaemonClient());
}

void NetworkHealthService::BindHealthReceiver(
    mojo::PendingReceiver<mojom::NetworkHealthService> receiver) {
  network_health_.BindReceiver(std::move(receiver));
}

void NetworkHealthService::BindDiagnosticsReceiver(
    mojo::PendingReceiver<
        network_diagnostics::mojom::NetworkDiagnosticsRoutines> receiver) {
  network_diagnostics_->BindReceiver(std::move(receiver));
}

NetworkHealthService* NetworkHealthService::GetInstance() {
  static base::NoDestructor<NetworkHealthService> instance;
  return instance.get();
}

}  // namespace network_health
}  // namespace chromeos
