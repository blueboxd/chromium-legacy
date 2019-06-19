// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"

#include "chromeos/network/network_device_handler.h"
#include "chromeos/services/network_config/cros_network_config.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_observer.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace network_config {

CrosNetworkConfigTestHelper::CrosNetworkConfigTestHelper() {
  network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
      network_state_helper_.network_state_handler());
  cros_network_config_impl_ = std::make_unique<CrosNetworkConfig>(
      network_state_helper_.network_state_handler(),
      network_device_handler_.get());
  SetupService();
}

CrosNetworkConfigTestHelper::~CrosNetworkConfigTestHelper() = default;

void CrosNetworkConfigTestHelper::SetupServiceInterface() {
  DCHECK(connector_);
  connector_->BindInterface(chromeos::network_config::mojom::kServiceName,
                            &service_interface_ptr_);
}

void CrosNetworkConfigTestHelper::SetupObserver() {
  DCHECK(service_interface_ptr_);
  observer_ = std::make_unique<CrosNetworkConfigTestObserver>();
  service_interface_ptr_->AddObserver(observer_->GenerateInterfacePtr());
}

void CrosNetworkConfigTestHelper::SetupService() {
  // Create a local service manager connector to handle requests.
  service_manager::mojom::ConnectorRequest request;
  connector_ = service_manager::Connector::Create(&request);
  connector_->OverrideBinderForTesting(
      service_manager::ServiceFilter::ByName(
          chromeos::network_config::mojom::kServiceName),
      mojom::CrosNetworkConfig::Name_,
      base::BindRepeating(&CrosNetworkConfigTestHelper::AddBinding,
                          base::Unretained(this)));
}

void CrosNetworkConfigTestHelper::AddBinding(
    mojo::ScopedMessagePipeHandle handle) {
  cros_network_config_impl_->BindRequest(
      mojom::CrosNetworkConfigRequest(std::move(handle)));
}

void CrosNetworkConfigTestHelper::FlushForTesting() {
  if (observer_)
    observer_->FlushForTesting();
}

}  // namespace network_config
}  // namespace chromeos
