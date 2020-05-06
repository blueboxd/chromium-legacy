// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_impl.h"

#include <memory>

#include "chrome/browser/chromeos/net/network_diagnostics/gateway_can_be_pinged_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/has_secure_wifi_connection_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/lan_connectivity_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/signal_strength_routine.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/device_event_log/device_event_log.h"

namespace chromeos {
namespace network_diagnostics {

NetworkDiagnosticsImpl::NetworkDiagnosticsImpl() {}

NetworkDiagnosticsImpl::~NetworkDiagnosticsImpl() {}

void NetworkDiagnosticsImpl::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkDiagnosticsRoutines> receiver) {
  NET_LOG(EVENT) << "NetworkDiagnosticsImpl::BindReceiver()";
  receivers_.Add(this, std::move(receiver));
}

void NetworkDiagnosticsImpl::LanConnectivity(LanConnectivityCallback callback) {
  LanConnectivityRoutine lan_connectivity_routine;
  lan_connectivity_routine.RunTest(std::move(callback));
}

void NetworkDiagnosticsImpl::SignalStrength(SignalStrengthCallback callback) {
  SignalStrengthRoutine signal_strength_routine;
  signal_strength_routine.RunTest(std::move(callback));
}

void NetworkDiagnosticsImpl::GatewayCanBePinged(
    GatewayCanBePingedCallback callback) {
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  GatewayCanBePingedRoutine gateway_can_be_pinged_routine(client);
  gateway_can_be_pinged_routine.RunTest(std::move(callback));
}

void NetworkDiagnosticsImpl::HasSecureWiFiConnection(
    HasSecureWiFiConnectionCallback callback) {
  HasSecureWiFiConnectionRoutine has_secure_wifi_connection_routine;
  has_secure_wifi_connection_routine.RunTest(std::move(callback));
}

}  // namespace network_diagnostics
}  // namespace chromeos
