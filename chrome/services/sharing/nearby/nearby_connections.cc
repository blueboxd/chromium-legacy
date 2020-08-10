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

namespace {

ConnectionRequestInfo CreateConnectionRequestInfo(
    const std::vector<uint8_t>& endpoint_info,
    mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener) {
  mojo::SharedRemote<mojom::ConnectionLifecycleListener> remote(
      std::move(listener));
  return ConnectionRequestInfo{
      .name = std::string(endpoint_info.begin(), endpoint_info.end()),
      .listener = {
          .initiated_cb =
              [remote](const std::string& endpoint_id,
                       const ConnectionResponseInfo& info) {
                if (!remote)
                  return;

                remote->OnConnectionInitiated(
                    endpoint_id,
                    mojom::ConnectionInfo::New(
                        info.authentication_token,
                        ByteArrayToMojom(info.raw_authentication_token),
                        ByteArrayToMojom(info.endpoint_info),
                        info.is_incoming_connection));
              },
          .accepted_cb =
              [remote](const std::string& endpoint_id) {
                if (!remote)
                  return;

                remote->OnConnectionAccepted(endpoint_id);
              },
          .rejected_cb =
              [remote](const std::string& endpoint_id, Status status) {
                if (!remote)
                  return;

                remote->OnConnectionRejected(endpoint_id,
                                             StatusToMojom(status.value));
              },
          .disconnected_cb =
              [remote](const std::string& endpoint_id) {
                if (!remote)
                  return;

                remote->OnDisconnected(endpoint_id);
              },
          .bandwidth_changed_cb =
              [remote](const std::string& endpoint_id, std::int32_t quality) {
                if (!remote)
                  return;

                remote->OnBandwidthChanged(endpoint_id, quality);
              },
      },
  };
}

}  // namespace

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

void NearbyConnections::StartAdvertising(
    const std::vector<uint8_t>& endpoint_info,
    const std::string& service_id,
    mojom::AdvertisingOptionsPtr options,
    mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener,
    StartAdvertisingCallback callback) {
  BooleanMediumSelector allowed_mediums = {
      .bluetooth = options->allowed_mediums->bluetooth,
      .web_rtc = options->allowed_mediums->web_rtc,
      .wifi_lan = options->allowed_mediums->wifi_lan,
  };
  ConnectionOptions connection_options{
      .strategy = StrategyFromMojom(options->strategy),
      .allowed = std::move(allowed_mediums),
      .auto_upgrade_bandwidth = options->auto_upgrade_bandwidth,
      .enforce_topology_constraints = options->enforce_topology_constraints,
  };

  core_->StartAdvertising(
      service_id, std::move(connection_options),
      CreateConnectionRequestInfo(endpoint_info, std::move(listener)),
      ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::StopAdvertising(StopAdvertisingCallback callback) {
  core_->StopAdvertising(ResultCallbackFromMojom(std::move(callback)));
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

void NearbyConnections::RequestConnection(
    const std::vector<uint8_t>& endpoint_info,
    const std::string& endpoint_id,
    mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener,
    RequestConnectionCallback callback) {
  core_->RequestConnection(
      endpoint_id,
      CreateConnectionRequestInfo(endpoint_info, std::move(listener)),
      ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::DisconnectFromEndpoint(
    const std::string& endpoint_id,
    DisconnectFromEndpointCallback callback) {
  core_->DisconnectFromEndpoint(endpoint_id,
                                ResultCallbackFromMojom(std::move(callback)));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
