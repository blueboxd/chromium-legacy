// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/standalone_connector_impl.h"

#include "base/logging.h"

namespace service_manager {

StandaloneConnectorImpl::StandaloneConnectorImpl(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

StandaloneConnectorImpl::~StandaloneConnectorImpl() = default;

mojo::PendingRemote<mojom::Connector> StandaloneConnectorImpl::MakeRemote() {
  mojo::PendingRemote<mojom::Connector> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void StandaloneConnectorImpl::BindInterface(
    const ServiceFilter& filter,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe,
    mojom::BindInterfacePriority priority,
    BindInterfaceCallback callback) {
  delegate_->OnConnect(
      filter.service_name(),
      mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe)));
  std::move(callback).Run(mojom::ConnectResult::SUCCEEDED, base::nullopt);
}

void StandaloneConnectorImpl::QueryService(const std::string& service_name,
                                           QueryServiceCallback callback) {
  NOTIMPLEMENTED()
      << "QueryService is not supported by StandaloneConnectorImpl.";
  std::move(callback).Run(nullptr);
}

void StandaloneConnectorImpl::WarmService(const ServiceFilter& filter,
                                          WarmServiceCallback callback) {
  NOTIMPLEMENTED()
      << "WarmService is not supported by StandaloneConnectorImpl.";
  std::move(callback).Run(mojom::ConnectResult::INVALID_ARGUMENT,
                          base::nullopt);
}

void StandaloneConnectorImpl::RegisterServiceInstance(
    const Identity& identity,
    mojo::ScopedMessagePipeHandle service_pipe,
    mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver,
    RegisterServiceInstanceCallback callback) {
  NOTIMPLEMENTED()
      << "RegisterServiceInstance is not supported by StandaloneConnectorImpl.";
  std::move(callback).Run(mojom::ConnectResult::INVALID_ARGUMENT);
}

void StandaloneConnectorImpl::Clone(mojom::ConnectorRequest request) {
  receivers_.Add(this, std::move(request));
}

void StandaloneConnectorImpl::FilterInterfaces(
    const std::string& spec,
    const Identity& source,
    mojo::PendingReceiver<mojom::InterfaceProvider> source_receiver,
    mojo::PendingRemote<mojom::InterfaceProvider> target) {
  NOTIMPLEMENTED()
      << "FilterInterfaces is not supported by StandaloneConnectorImpl.";
}

}  // namespace service_manager
