// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace network_diagnostics {

class NetworkDiagnosticsImpl : public mojom::NetworkDiagnosticsRoutines {
 public:
  NetworkDiagnosticsImpl();
  ~NetworkDiagnosticsImpl() override;

  // Binds this instance, an implementation of
  // chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines, to
  // multiple mojom::NetworkDiagnosticsRoutines receivers.
  void BindReceiver(
      mojo::PendingReceiver<mojom::NetworkDiagnosticsRoutines> receiver);

  // mojom::NetworkDiagnostics
  void LanConnectivity(LanConnectivityCallback callback) override;
  void SignalStrength(SignalStrengthCallback callback) override;
  void GatewayCanBePinged(GatewayCanBePingedCallback callback) override;
  void HasSecureWiFiConnection(
      HasSecureWiFiConnectionCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::NetworkDiagnosticsRoutines> receivers_;
  base::WeakPtrFactory<NetworkDiagnosticsImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkDiagnosticsImpl);
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_IMPL_H_
