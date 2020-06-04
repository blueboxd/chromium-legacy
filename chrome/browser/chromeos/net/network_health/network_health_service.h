// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_

#include "chrome/browser/chromeos/net/network_health/network_health.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace chromeos {
namespace network_health {

class NetworkHealthService {
 public:
  static NetworkHealthService* GetInstance();

  NetworkHealthService();
  ~NetworkHealthService() = delete;

  void BindRemote(mojo::PendingReceiver<mojom::NetworkHealthService> receiver);

 private:
  NetworkHealth network_health_;
};

}  // namespace network_health
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_
