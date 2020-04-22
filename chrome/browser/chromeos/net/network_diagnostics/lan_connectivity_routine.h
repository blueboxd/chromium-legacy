// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_LAN_CONNECTIVITY_ROUTINE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_LAN_CONNECTIVITY_ROUTINE_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace network_diagnostics {

// Tests whether the device is connected to a LAN. It is possible that the
// device may be trapped in a captive portal yet pass this test successfully.
// Captive portal checks are done separately and are outside of the scope of
// this routine.
class LanConnectivityRoutine : public NetworkDiagnosticsRoutine {
 public:
  using LanConnectivityRoutineCallback =
      base::OnceCallback<void(mojom::RoutineVerdict)>;

  LanConnectivityRoutine();
  ~LanConnectivityRoutine() override;

  // NetworkDiagnosticsRoutine:
  bool CanRun() override;
  void AnalyzeResultsAndExecuteCallback() override;

  // Run the core logic of this routine. Set |callback| to
  // |routine_completed_callback_|, which is to be executed in
  // AnalyzeResultsAndExecuteCallback().
  void RunTest(LanConnectivityRoutineCallback callback);

 private:
  void FetchActiveNetworks();
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  bool lan_connected_ = false;
  LanConnectivityRoutineCallback routine_completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(LanConnectivityRoutine);
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_LAN_CONNECTIVITY_ROUTINE_H_
