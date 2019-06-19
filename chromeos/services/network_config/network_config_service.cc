// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/network_config_service.h"

#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/services/network_config/cros_network_config.h"
#include "components/device_event_log/device_event_log.h"

namespace chromeos {

namespace network_config {

NetworkConfigService::NetworkConfigService(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      cros_network_config_(std::make_unique<CrosNetworkConfig>(
          NetworkHandler::Get()->network_state_handler(),
          NetworkHandler::Get()->network_device_handler())) {}

NetworkConfigService::~NetworkConfigService() = default;

void NetworkConfigService::OnStart() {
  NET_LOG(EVENT) << "NetworkConfigService::OnStart()";
  registry_.AddInterface(
      base::BindRepeating(&CrosNetworkConfig::BindRequest,
                          base::Unretained(cros_network_config_.get())));
}

void NetworkConfigService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  NET_LOG(EVENT) << "NetworkConfigService::OnBindInterface() from interface: "
                 << interface_name;
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace network_config

}  // namespace chromeos
