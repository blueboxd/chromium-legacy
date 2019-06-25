// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/network_provider_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "services/service_manager/public/cpp/connector.h"

using ConnectionStatus = assistant_client::NetworkProvider::ConnectionStatus;
using NetworkStatePropertiesPtr =
    chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ConnectionStateType =
    chromeos::network_config::mojom::ConnectionStateType;

namespace chromeos {
namespace assistant {

NetworkProviderImpl::NetworkProviderImpl(service_manager::Connector* connector)
    : connection_status_(ConnectionStatus::UNKNOWN), binding_(this) {
  // |connector| can be null for the unittests
  if (connector)
    Init(connector);
}

NetworkProviderImpl::~NetworkProviderImpl() = default;

network_config::mojom::CrosNetworkConfigObserverPtr
NetworkProviderImpl::BindAndGetPtr() {
  DCHECK(!binding_.is_bound());
  network_config::mojom::CrosNetworkConfigObserverPtr observer_ptr;
  binding_.Bind(mojo::MakeRequest(&observer_ptr));
  return observer_ptr;
}

void NetworkProviderImpl::OnActiveNetworksChanged(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  const bool is_any_network_online =
      std::any_of(networks.begin(), networks.end(), [](const auto& network) {
        return network->connection_state == ConnectionStateType::kOnline;
      });

  if (is_any_network_online)
    connection_status_ = ConnectionStatus::CONNECTED;
  else
    connection_status_ = ConnectionStatus::DISCONNECTED_FROM_INTERNET;
}

void NetworkProviderImpl::Init(service_manager::Connector* connector) {
  BindCrosNetworkConfig(connector);
  AddAndFireCrosNetworkConfigObserver();
}

void NetworkProviderImpl::BindCrosNetworkConfig(
    service_manager::Connector* connector) {
  DCHECK(!cros_network_config_ptr_.is_bound());
  connector->BindInterface(chromeos::network_config::mojom::kServiceName,
                           &cros_network_config_ptr_);
}

void NetworkProviderImpl::AddAndFireCrosNetworkConfigObserver() {
  cros_network_config_ptr_->AddObserver(BindAndGetPtr());
  cros_network_config_ptr_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kActive,
          network_config::mojom::NetworkType::kAll,
          network_config::mojom::kNoLimit),
      base::BindOnce(&NetworkProviderImpl::OnActiveNetworksChanged,
                     base::Unretained(this)));
}

ConnectionStatus NetworkProviderImpl::GetConnectionStatus() {
  return connection_status_;
}

// Mdns responder is not supported in ChromeOS.
assistant_client::MdnsResponder* NetworkProviderImpl::GetMdnsResponder() {
  return nullptr;
}

}  // namespace assistant
}  // namespace chromeos
