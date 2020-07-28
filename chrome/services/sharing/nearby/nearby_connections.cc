// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections.h"

#include "base/task/post_task.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/services/sharing/nearby/nearby_connections_conversions.h"
#include "chrome/services/sharing/public/mojom/nearby_connections_types.mojom.h"

namespace location {
namespace nearby {
namespace connections {

// Should only be accessed by objects within lifetime of NearbyConnections.
NearbyConnections* g_instance = nullptr;

// static
NearbyConnections& NearbyConnections::GetInstance() {
  DCHECK(g_instance);
  return *g_instance;
}

NearbyConnections::NearbyConnections(
    mojo::PendingReceiver<mojom::NearbyConnections> nearby_connections,
    mojom::NearbyConnectionsDependenciesPtr dependencies,
    base::OnceClosure on_disconnect,
    std::unique_ptr<Core> core)
    : nearby_connections_(this, std::move(nearby_connections)),
      on_disconnect_(std::move(on_disconnect)),
      core_(std::move(core)) {
  nearby_connections_.set_disconnect_handler(base::BindOnce(
      &NearbyConnections::OnDisconnect, weak_ptr_factory_.GetWeakPtr()));

  if (dependencies->bluetooth_adapter) {
    bluetooth_adapter_.Bind(std::move(dependencies->bluetooth_adapter),
                            /*bind_task_runner=*/nullptr);
    bluetooth_adapter_.set_disconnect_handler(
        base::BindOnce(&NearbyConnections::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr()),
        base::SequencedTaskRunnerHandle::Get());
  }

  socket_manager_.Bind(
      std::move(dependencies->webrtc_dependencies->socket_manager),
      /*bind_task_runner=*/nullptr);
  socket_manager_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  mdns_responder_.Bind(
      std::move(dependencies->webrtc_dependencies->mdns_responder),
      /*bind_task_runner=*/nullptr);
  mdns_responder_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  ice_config_fetcher_.Bind(
      std::move(dependencies->webrtc_dependencies->ice_config_fetcher),
      /*bind_task_runner=*/nullptr);
  ice_config_fetcher_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  webrtc_signaling_messenger_.Bind(
      std::move(dependencies->webrtc_dependencies->messenger),
      /*bind_task_runner=*/nullptr);
  webrtc_signaling_messenger_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  // There should only be one instance of NearbyConnections in a process.
  DCHECK(!g_instance);
  g_instance = this;
}

NearbyConnections::~NearbyConnections() {
  core_.reset();
  g_instance = nullptr;
}

void NearbyConnections::OnDisconnect() {
  if (on_disconnect_)
    std::move(on_disconnect_).Run();
  // Note: |this| might be destroyed here.
}

bluetooth::mojom::Adapter* NearbyConnections::GetBluetoothAdapter() {
  if (!bluetooth_adapter_.is_bound())
    return nullptr;

  return bluetooth_adapter_.get();
}

network::mojom::P2PSocketManager*
NearbyConnections::GetWebRtcP2PSocketManager() {
  if (!socket_manager_.is_bound())
    return nullptr;

  return socket_manager_.get();
}

network::mojom::MdnsResponder* NearbyConnections::GetWebRtcMdnsResponder() {
  if (!mdns_responder_.is_bound())
    return nullptr;

  return mdns_responder_.get();
}

sharing::mojom::IceConfigFetcher*
NearbyConnections::GetWebRtcIceConfigFetcher() {
  if (!ice_config_fetcher_.is_bound())
    return nullptr;

  return ice_config_fetcher_.get();
}

sharing::mojom::WebRtcSignalingMessenger*
NearbyConnections::GetWebRtcSignalingMessenger() {
  if (!webrtc_signaling_messenger_.is_bound())
    return nullptr;

  return webrtc_signaling_messenger_.get();
}

void NearbyConnections::StartDiscovery(
    const std::string& service_id,
    mojom::DiscoveryOptionsPtr options,
    mojo::PendingRemote<mojom::EndpointDiscoveryListener> listener,
    StartDiscoveryCallback callback) {
  ConnectionOptions connection_options{
      .strategy = StrategyFromMojom(options->strategy)};
  mojo::SharedRemote<mojom::EndpointDiscoveryListener> remote(
      std::move(listener));
  DiscoveryListener discovery_listener{
      .endpoint_found_cb =
          [remote](const std::string& endpoint_id,
                   const std::string& endpoint_name,
                   const std::string& service_id) {
            if (!remote) {
              return;
            }

            remote->OnEndpointFound(
                endpoint_id, mojom::DiscoveredEndpointInfo::New(
                                 std::vector<uint8_t>(endpoint_name.begin(),
                                                      endpoint_name.end()),
                                 service_id));
          },
      .endpoint_lost_cb =
          [remote](const std::string& endpoint_id) {
            if (!remote)
              return;

            remote->OnEndpointLost(endpoint_id);
          },
  };
  ResultCallback result_callback = ResultCallbackFromMojom(std::move(callback));

  core_->StartDiscovery(service_id, std::move(connection_options),
                        std::move(discovery_listener),
                        std::move(result_callback));
}

void NearbyConnections::StopDiscovery(StopDiscoveryCallback callback) {
  core_->StopDiscovery(ResultCallbackFromMojom(std::move(callback)));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
