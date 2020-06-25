// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_

#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_impl.h"
#include "chrome/browser/chromeos/net/network_health/network_health.h"

namespace chromeos {
namespace network_health {

class NetworkHealthService {
 public:
  static NetworkHealthService* GetInstance();

  NetworkHealthService();
  ~NetworkHealthService() = delete;

  void BindRemote(mojo::PendingReceiver<mojom::NetworkHealthService> receiver);
  void BindDiagnosticsRemote(
      mojo::PendingReceiver<
          network_diagnostics::mojom::NetworkDiagnosticsRoutines> receiver);

 private:
  NetworkHealth network_health_;
  network_diagnostics::NetworkDiagnosticsImpl network_diagnostics_;
};

}  // namespace network_health
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_
