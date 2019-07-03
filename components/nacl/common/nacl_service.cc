// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_service.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/service_names.mojom.h"
#include "ipc/ipc.mojom.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service_binding.h"

#if defined(OS_POSIX)
#include "base/files/scoped_file.h"
#include "base/posix/global_descriptors.h"
#include "services/service_manager/embedder/descriptors.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mach_port_rendezvous.h"
#endif

namespace {

mojo::IncomingInvitation EstablishMojoConnection() {
  mojo::PlatformChannelEndpoint endpoint;
#if defined(OS_WIN)
  endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
#elif defined(OS_MACOSX)
  auto* client = base::MachPortRendezvousClient::GetInstance();
  if (client) {
    endpoint = mojo::PlatformChannelEndpoint(
        mojo::PlatformHandle(client->TakeReceiveRight('mojo')));
  }
#else
  endpoint = mojo::PlatformChannelEndpoint(mojo::PlatformHandle(
      base::ScopedFD(base::GlobalDescriptors::GetInstance()->Get(
          service_manager::kMojoIPCChannel))));
#endif  // !defined(OS_WIN)
  DCHECK(endpoint.is_valid());
  return mojo::IncomingInvitation::Accept(std::move(endpoint));
}

service_manager::mojom::ServiceRequest ConnectToServiceManager(
    mojo::IncomingInvitation* invitation) {
  const std::string service_request_channel_token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          service_manager::switches::kServiceRequestChannelToken);
  DCHECK(!service_request_channel_token.empty());
  mojo::ScopedMessagePipeHandle parent_handle =
      invitation->ExtractMessagePipe(service_request_channel_token);
  DCHECK(parent_handle.is_valid());
  return service_manager::mojom::ServiceRequest(std::move(parent_handle));
}

class NaClService : public service_manager::Service {
 public:
  NaClService(service_manager::mojom::ServiceRequest request,
              IPC::mojom::ChannelBootstrapPtrInfo bootstrap,
              std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support)
      : service_binding_(this, std::move(request)),
        ipc_channel_bootstrap_(std::move(bootstrap)),
        ipc_support_(std::move(ipc_support)) {}

  ~NaClService() override = default;

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    if (source_info.identity.name() == content::mojom::kSystemServiceName &&
        interface_name == IPC::mojom::ChannelBootstrap::Name_ && !connected_) {
      connected_ = true;
      mojo::FuseInterface(
          IPC::mojom::ChannelBootstrapRequest(std::move(interface_pipe)),
          std::move(ipc_channel_bootstrap_));
    } else {
      DVLOG(1) << "Ignoring request for unknown interface " << interface_name;
    }
  }

 private:
  service_manager::ServiceBinding service_binding_;
  IPC::mojom::ChannelBootstrapPtrInfo ipc_channel_bootstrap_;
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;
  bool connected_ = false;

  DISALLOW_COPY_AND_ASSIGN(NaClService);
};

}  // namespace

std::unique_ptr<service_manager::Service> CreateNaClService(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    mojo::ScopedMessagePipeHandle* ipc_channel) {
  auto ipc_support = std::make_unique<mojo::core::ScopedIPCSupport>(
      std::move(io_task_runner),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);
  auto invitation = EstablishMojoConnection();
  IPC::mojom::ChannelBootstrapPtr bootstrap;
  *ipc_channel = mojo::MakeRequest(&bootstrap).PassMessagePipe();
  return std::make_unique<NaClService>(ConnectToServiceManager(&invitation),
                                       bootstrap.PassInterface(),
                                       std::move(ipc_support));
}
