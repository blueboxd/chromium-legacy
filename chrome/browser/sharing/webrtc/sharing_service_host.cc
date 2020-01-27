// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/webrtc/sharing_service_host.h"

#include "base/callback.h"
#include "base/guid.h"
#include "chrome/browser/sharing/webrtc/sharing_mojo_service.h"
#include "chrome/browser/sharing/webrtc/sharing_webrtc_connection_host.h"
#include "chrome/browser/sharing/webrtc/webrtc_signalling_host_fcm.h"
#include "content/public/browser/network_context_client_base.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"
#include "url/gurl.h"

namespace {

// static
std::unique_ptr<syncer::DeviceInfo> CreateDeviceInfo(
    const std::string& device_guid,
    const syncer::DeviceInfo::SharingTargetInfo& target_info) {
  return std::make_unique<syncer::DeviceInfo>(
      device_guid, /*client_name=*/std::string(),
      /*chrome_version=*/std::string(), /*sync_user_agent=*/std::string(),
      /*device_type=*/sync_pb::SyncEnums::TYPE_UNSET,
      /*signin_scoped_device_id=*/std::string(),
      /*hardware_info=*/base::SysInfo::HardwareInfo(),
      /*last_updated_timestamp=*/base::Time(),
      /*send_tab_to_self_receiving_enabled=*/true,
      syncer::DeviceInfo::SharingInfo(target_info, /*sender_id_target_info=*/{},
                                      /*enabled_features=*/{}));
}

template <typename T>
struct MojoPipe {
  mojo::PendingRemote<T> remote;
  mojo::PendingReceiver<T> receiver{remote.InitWithNewPipeAndPassReceiver()};
};

// static
// This is called from the sandboxed process after sending a message.
void OnMessageSent(
    SharingMessageSender::SendMessageDelegate::SendMessageCallback callback,
    std::string message_guid,
    sharing::mojom::SendMessageResult result) {
  switch (result) {
    case sharing::mojom::SendMessageResult::kSuccess:
      std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                              message_guid);
      break;
    case sharing::mojom::SendMessageResult::kError:
      std::move(callback).Run(SharingSendMessageResult::kInternalError,
                              base::nullopt);
      break;
  }
}

}  // namespace

struct SharingWebRtcMojoPipes {
  MojoPipe<sharing::mojom::SignallingSender> signalling_sender;
  MojoPipe<sharing::mojom::SignallingReceiver> signalling_receiver;
  MojoPipe<sharing::mojom::SharingWebRtcConnectionDelegate> delegate;
  MojoPipe<sharing::mojom::SharingWebRtcConnection> connection;
  MojoPipe<network::mojom::P2PTrustedSocketManagerClient> socket_manager_client;
  MojoPipe<network::mojom::P2PTrustedSocketManager> trusted_socket_manager;
  MojoPipe<network::mojom::P2PSocketManager> socket_manager;
  MojoPipe<network::mojom::MdnsResponder> mdns_responder;
};

SharingServiceHost::SharingServiceHost(
    SharingMessageSender* message_sender,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : message_sender_(message_sender),
      ice_config_fetcher_(url_loader_factory) {}

SharingServiceHost::~SharingServiceHost() = default;

void SharingServiceHost::DoSendMessageToDevice(
    const syncer::DeviceInfo& device,
    base::TimeDelta time_to_live,
    chrome_browser_sharing::SharingMessage message,
    SendMessageCallback callback) {
  chrome_browser_sharing::WebRtcMessage webrtc_message;
  std::string message_guid = base::GenerateGUID();
  webrtc_message.mutable_message()->Swap(&message);
  webrtc_message.set_message_guid(message_guid);

  // TODO(crbug.com/1044539): support multiple messages over the same connection
  // or queue messages instead of rejecting them here.
  auto connection_iter = connections_.find(device.guid());
  if (connection_iter != connections_.end()) {
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            std::string());
    return;
  }

  // Remote device must have a valid sharing_info.
  if (!device.sharing_info()) {
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            std::string());
    return;
  }

  GetConnection(device.guid(), device.sharing_info()->vapid_target_info)
      ->SendMessage(std::move(webrtc_message),
                    base::BindOnce(&OnMessageSent, std::move(callback),
                                   std::move(message_guid)));
}

void SharingServiceHost::OnPeerConnectionClosed(
    const std::string& device_guid) {
  connections_.erase(device_guid);
  if (connections_.empty())
    sharing_utility_service_.reset();
}

// |callback| will be called from the sandboxed process with the remote answer.
void SharingServiceHost::OnOfferReceived(
    const std::string& device_guid,
    const syncer::DeviceInfo::SharingTargetInfo& target_info,
    const std::string& offer,
    base::OnceCallback<void(const std::string&)> callback) {
  GetConnection(device_guid, target_info)
      ->OnOfferReceived(offer, std::move(callback));
}

void SharingServiceHost::OnIceCandidatesReceived(
    const std::string& device_guid,
    const syncer::DeviceInfo::SharingTargetInfo& target_info,
    std::vector<sharing::mojom::IceCandidatePtr> ice_candidates) {
  GetConnection(device_guid, target_info)
      ->OnIceCandidatesReceived(std::move(ice_candidates));
}

void SharingServiceHost::SetSharingHandlerRegistry(
    SharingHandlerRegistry* handler_registry) {
  handler_registry_ = handler_registry;
}

SharingWebRtcConnectionHost* SharingServiceHost::GetConnection(
    const std::string& device_guid,
    const syncer::DeviceInfo::SharingTargetInfo& target_info) {
  auto connection_iter = connections_.find(device_guid);
  if (connection_iter != connections_.end())
    return connection_iter->second.get();

  auto pipes = std::make_unique<SharingWebRtcMojoPipes>();

  auto signalling_host = std::make_unique<WebRtcSignallingHostFCM>(
      std::move(pipes->signalling_sender.receiver),
      std::move(pipes->signalling_receiver.remote), message_sender_,
      CreateDeviceInfo(device_guid, target_info));

  // base::Unretained is safe as the connection is owned by |this|.
  auto result = connections_.emplace(
      device_guid,
      std::make_unique<SharingWebRtcConnectionHost>(
          std::move(signalling_host), handler_registry_,
          CreateDeviceInfo(device_guid, target_info),
          base::BindOnce(&SharingServiceHost::OnPeerConnectionClosed,
                         base::Unretained(this)),
          std::move(pipes->delegate.receiver),
          std::move(pipes->connection.remote),
          std::move(pipes->socket_manager_client.receiver),
          std::move(pipes->trusted_socket_manager.remote)));
  DCHECK(result.second);

  GetNetworkContext()->CreateP2PSocketManager(
      std::move(pipes->socket_manager_client.remote),
      std::move(pipes->trusted_socket_manager.receiver),
      std::move(pipes->socket_manager.receiver));
  GetNetworkContext()->CreateMdnsResponder(
      std::move(pipes->mdns_responder.receiver));

  ice_config_fetcher_.GetIceServers(
      base::BindOnce(&SharingServiceHost::OnIceServersReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(pipes)));

  return result.first->second.get();
}

void SharingServiceHost::OnIceServersReceived(
    std::unique_ptr<SharingWebRtcMojoPipes> pipes,
    std::vector<sharing::mojom::IceServerPtr> ice_servers) {
  if (!sharing_utility_service_) {
    sharing_utility_service_.Bind(sharing::LaunchSharing());
    sharing_utility_service_.reset_on_disconnect();
  }

  sharing_utility_service_->CreateSharingWebRtcConnection(
      std::move(pipes->signalling_sender.remote),
      std::move(pipes->signalling_receiver.receiver),
      std::move(pipes->delegate.remote), std::move(pipes->connection.receiver),
      std::move(pipes->socket_manager.remote),
      std::move(pipes->mdns_responder.remote), std::move(ice_servers));
}

network::mojom::NetworkContext* SharingServiceHost::GetNetworkContext() {
  if (network_context_ && network_context_.is_connected())
    return network_context_.get();

  network_context_.reset();

  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->user_agent = "";
  context_params->accept_language = "en-us,en";

  content::GetNetworkService()->CreateNetworkContext(
      network_context_.BindNewPipeAndPassReceiver(), std::move(context_params));

  mojo::PendingRemote<network::mojom::NetworkContextClient> client_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<content::NetworkContextClientBase>(),
      client_remote.InitWithNewPipeAndPassReceiver());
  network_context_->SetClient(std::move(client_remote));

  return network_context_.get();
}
