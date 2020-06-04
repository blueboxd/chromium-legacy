// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_health/network_health_service.h"

#include "base/no_destructor.h"

#include "chrome/browser/chromeos/net/network_health/network_health.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace chromeos {
namespace network_health {

NetworkHealthService::NetworkHealthService() = default;

void NetworkHealthService::BindRemote(
    mojo::PendingReceiver<mojom::NetworkHealthService> receiver) {
  network_health_.BindRemote(std::move(receiver));
}

NetworkHealthService* NetworkHealthService::GetInstance() {
  static base::NoDestructor<NetworkHealthService> instance;
  return instance.get();
}

}  // namespace network_health
}  // namespace chromeos
