// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/external_service_support/external_connector_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/external_mojo/external_service_support/external_service.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"

namespace chromecast {
namespace external_service_support {

namespace {
constexpr base::TimeDelta kConnectRetryDelay =
    base::TimeDelta::FromMilliseconds(500);
}  // namespace

// static
void ExternalConnector::Connect(
    const std::string& broker_path,
    base::OnceCallback<void(std::unique_ptr<ExternalConnector>)> callback) {
  DCHECK(callback);
  std::move(callback).Run(Create(broker_path));
}

// static
std::unique_ptr<ExternalConnector> ExternalConnector::Create(
    const std::string& broker_path) {
  return std::make_unique<ExternalConnectorImpl>(broker_path);
}

ExternalConnectorImpl::ExternalConnectorImpl(const std::string& broker_path)
    : broker_path_(broker_path) {
  InitializeBrokerConnection();
}

ExternalConnectorImpl::ExternalConnectorImpl(
    const std::string& broker_path,
    mojo::PendingRemote<external_mojo::mojom::ExternalConnector>
        connector_pending_remote)
    : broker_path_(broker_path),
      connector_pending_remote_from_clone_(
          std::move(connector_pending_remote)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ExternalConnectorImpl::~ExternalConnectorImpl() = default;

std::unique_ptr<base::CallbackList<void()>::Subscription>
ExternalConnectorImpl::AddConnectionErrorCallback(
    base::RepeatingClosure callback) {
  return error_callbacks_.Add(std::move(callback));
}

void ExternalConnectorImpl::RegisterService(const std::string& service_name,
                                            ExternalService* service) {
  RegisterService(service_name, service->GetReceiver());
}

void ExternalConnectorImpl::RegisterService(
    const std::string& service_name,
    mojo::PendingRemote<external_mojo::mojom::ExternalService> service_remote) {
  if (BindConnectorIfNecessary()) {
    connector_->RegisterServiceInstance(service_name,
                                        std::move(service_remote));
  }
}

void ExternalConnectorImpl::QueryServiceList(
    base::OnceCallback<void(
        std::vector<chromecast::external_mojo::mojom::ExternalServiceInfoPtr>)>
        callback) {
  if (BindConnectorIfNecessary()) {
    connector_->QueryServiceList(std::move(callback));
  }
}

void ExternalConnectorImpl::BindInterface(
    const std::string& service_name,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe,
    bool async) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!async) {
    BindInterfaceImmediately(service_name, interface_name,
                             std::move(interface_pipe));
    return;
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExternalConnectorImpl::BindInterfaceImmediately,
                     weak_factory_.GetWeakPtr(), service_name, interface_name,
                     std::move(interface_pipe)));
}

void ExternalConnectorImpl::BindInterfaceImmediately(
    const std::string& service_name,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (BindConnectorIfNecessary()) {
    connector_->BindInterface(service_name, interface_name,
                              std::move(interface_pipe));
  }
}

std::unique_ptr<ExternalConnector> ExternalConnectorImpl::Clone() {
  mojo::PendingRemote<external_mojo::mojom::ExternalConnector> connector_remote;
  auto receiver = connector_remote.InitWithNewPipeAndPassReceiver();
  if (BindConnectorIfNecessary()) {
    connector_->Clone(std::move(receiver));
  }
  return std::make_unique<ExternalConnectorImpl>(broker_path_,
                                                 std::move(connector_remote));
}

void ExternalConnectorImpl::SendChromiumConnectorRequest(
    mojo::ScopedMessagePipeHandle request) {
  if (BindConnectorIfNecessary()) {
    connector_->BindChromiumConnector(std::move(request));
  }
}

void ExternalConnectorImpl::OnMojoDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connector_.reset();
  connector_pending_remote_from_clone_.reset();
  connector_pending_receiver_for_broker_.reset();
  InitializeBrokerConnection();
  error_callbacks_.Notify();
}

bool ExternalConnectorImpl::BindConnectorIfNecessary() {
  // Bind the message pipe and SequenceChecker to the current thread the first
  // time it is used to connect.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (connector_.is_bound()) {
    return true;
  }

  DCHECK(connector_pending_remote_from_clone_.is_valid());

  connector_.Bind(std::move(connector_pending_remote_from_clone_));
  connector_.set_disconnect_handler(base::BindOnce(
      &ExternalConnectorImpl::OnMojoDisconnect, base::Unretained(this)));

  return true;
}

void ExternalConnectorImpl::InitializeBrokerConnection() {
  // Setup the connector_ to be valid and bound.
  // Once |connector_pending_receiver_for_broker_| is bound in the mojo broker,
  // messages will be delivered.
  mojo::PendingRemote<external_mojo::mojom::ExternalConnector> connector_remote;
  connector_pending_receiver_for_broker_ =
      connector_remote.InitWithNewPipeAndPassReceiver();
  connector_.Bind(std::move(connector_remote));
  connector_.set_disconnect_handler(base::BindOnce(
      &ExternalConnectorImpl::OnMojoDisconnect, base::Unretained(this)));

  AttemptBrokerConnection();
}

void ExternalConnectorImpl::AttemptBrokerConnection() {
  mojo::PlatformChannelEndpoint endpoint =
      mojo::NamedPlatformChannel::ConnectToServer(broker_path_);
  if (!endpoint.is_valid()) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExternalConnectorImpl::AttemptBrokerConnection,
                       weak_factory_.GetWeakPtr()),
        kConnectRetryDelay);
    return;
  }

  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  auto remote_pipe = invitation.ExtractMessagePipe(0);
  if (!remote_pipe) {
    LOG(ERROR) << "Invalid message pipe";
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExternalConnectorImpl::AttemptBrokerConnection,
                       weak_factory_.GetWeakPtr()),
        kConnectRetryDelay);
    return;
  }

  mojo::FuseMessagePipes(connector_pending_receiver_for_broker_.PassPipe(),
                         std::move(remote_pipe));
}

}  // namespace external_service_support
}  // namespace chromecast
