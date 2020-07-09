// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cfm/public/cpp/service_connection.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/dbus/cfm/cfm_hotline_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace chromeos {
namespace cfm {

namespace {

// Real Impl of ServiceConnection
class ServiceConnectionImpl : public ServiceConnection {
 public:
  ServiceConnectionImpl();
  ServiceConnectionImpl(const ServiceConnectionImpl&) = delete;
  ServiceConnectionImpl& operator=(const ServiceConnectionImpl&) = delete;
  ~ServiceConnectionImpl() override = default;

 private:
  void BindServiceContext(
      mojo::PendingReceiver<::chromeos::cfm::mojom::CfmServiceContext> receiver)
      override;

  void CfMContextServiceStarted(
      mojo::PendingReceiver<::chromeos::cfm::mojom::CfmServiceContext> receiver,
      bool is_available);

  // Response callback for CfmHotlineClient::BootstrapMojoConnection.
  void OnBootstrapMojoConnectionResponse(
      mojo::PendingReceiver<::chromeos::cfm::mojom::CfmServiceContext> receiver,
      mojo::PlatformChannel channel,
      mojo::ScopedMessagePipeHandle context_remote_pipe,
      bool success);

  SEQUENCE_CHECKER(sequence_checker_);
};

void ServiceConnectionImpl::BindServiceContext(
    mojo::PendingReceiver<::chromeos::cfm::mojom::CfmServiceContext> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CfmHotlineClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&ServiceConnectionImpl::CfMContextServiceStarted,
                     base::Unretained(this), std::move(receiver)));
}

void ServiceConnectionImpl::CfMContextServiceStarted(
    mojo::PendingReceiver<::chromeos::cfm::mojom::CfmServiceContext> receiver,
    bool is_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_available) {
    LOG(WARNING) << "CfmServiceContext not available.";
    receiver.reset();
    return;
  }

  mojo::PlatformChannel channel;

  // Invite the Chromium OS service to the Chromium IPC network
  // Prepare a Mojo invitation to send through |platform_channel|.
  mojo::OutgoingInvitation invitation;
  // Include an initial Mojo pipe in the invitation.
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(0u);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());

  // Send the file descriptor for the other end of |platform_channel| to the
  // Cfm service daemon over D-Bus.
  CfmHotlineClient::Get()->BootstrapMojoConnection(
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
      base::BindOnce(&ServiceConnectionImpl::OnBootstrapMojoConnectionResponse,
                     base::Unretained(this), std::move(receiver),
                     std::move(channel), std::move(pipe)));
}

void ServiceConnectionImpl::OnBootstrapMojoConnectionResponse(
    mojo::PendingReceiver<::chromeos::cfm::mojom::CfmServiceContext> receiver,
    mojo::PlatformChannel channel,
    mojo::ScopedMessagePipeHandle context_remote_pipe,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    LOG(WARNING) << "BootstrapMojoConnection D-Bus call failed";
    receiver.reset();
    return;
  }

  MojoResult result = mojo::FuseMessagePipes(receiver.PassPipe(),
                                             std::move(context_remote_pipe));
  if (result == MOJO_RESULT_OK) {
    LOG(WARNING) << "Fusing message pipes failed.";
  }
}

ServiceConnectionImpl::ServiceConnectionImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

static ServiceConnection* g_fake_service_connection_for_testing = nullptr;

}  // namespace

ServiceConnection* ServiceConnection::GetInstance() {
  if (g_fake_service_connection_for_testing) {
    return g_fake_service_connection_for_testing;
  }
  static base::NoDestructor<ServiceConnectionImpl> service_connection;
  return service_connection.get();
}

void ServiceConnection::UseFakeServiceConnectionForTesting(
    ServiceConnection* const fake_service_connection) {
  g_fake_service_connection_for_testing = fake_service_connection;
}

}  // namespace cfm
}  // namespace chromeos
