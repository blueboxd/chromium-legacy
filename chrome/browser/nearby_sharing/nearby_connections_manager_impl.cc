// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_connections_manager_impl.h"

#include "base/strings/string_number_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/services/sharing/public/mojom/nearby_connections_types.mojom.h"
#include "crypto/random.h"

namespace {

const char kServiceId[] = "NearbySharing";
const location::nearby::connections::mojom::Strategy kStrategy =
    location::nearby::connections::mojom::Strategy::kP2pPointToPoint;

}  // namespace

NearbyConnectionsManagerImpl::NearbyConnectionsManagerImpl(
    NearbyProcessManager* process_manager,
    Profile* profile)
    : process_manager_(process_manager), profile_(profile) {
  DCHECK(process_manager_);
  DCHECK(profile_);
  nearby_process_observer_.Add(process_manager_);
}

NearbyConnectionsManagerImpl::~NearbyConnectionsManagerImpl() = default;

void NearbyConnectionsManagerImpl::Shutdown() {
  // TOOD(crbug/1076008): Implement.
}

void NearbyConnectionsManagerImpl::StartAdvertising(
    std::vector<uint8_t> endpoint_info,
    IncomingConnectionListener* listener,
    PowerLevel power_level,
    DataUsage data_usage,
    ConnectionsCallback callback) {
  if (!BindNearbyConnections()) {
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  // TOOD(crbug/1076008): nearby_connections_->StartAdvertising
}

void NearbyConnectionsManagerImpl::StopAdvertising() {
  if (!nearby_connections_)
    return;
}

void NearbyConnectionsManagerImpl::StartDiscovery(
    DiscoveryListener* listener,
    ConnectionsCallback callback) {
  DCHECK(listener);
  DCHECK(!discovery_listener_);

  if (!BindNearbyConnections()) {
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  discovery_listener_ = listener;
  nearby_connections_->StartDiscovery(
      kServiceId, DiscoveryOptions::New(kStrategy),
      endpoint_discovery_listener_.BindNewPipeAndPassRemote(),
      std::move(callback));
}

void NearbyConnectionsManagerImpl::StopDiscovery() {
  if (nearby_connections_)
    nearby_connections_->StopDiscovery(base::DoNothing());

  discovered_endpoints_.clear();
  discovery_listener_ = nullptr;
  endpoint_discovery_listener_.reset();
}

void NearbyConnectionsManagerImpl::Connect(
    std::vector<uint8_t> endpoint_info,
    const std::string& endpoint_id,
    base::Optional<std::vector<uint8_t>> bluetooth_mac_address,
    DataUsage data_usage,
    NearbyConnectionCallback callback) {
  // TOOD(crbug/1076008): Implement.
  if (!nearby_connections_) {
    std::move(callback).Run(nullptr);
    return;
  }

  // TODO(crbug/10706008): Add MediumSelector and bluetooth_mac_address.
  nearby_connections_->RequestConnection(
      endpoint_info, endpoint_id,
      connection_lifecycle_listener_.BindNewPipeAndPassRemote(),
      base::BindOnce(&NearbyConnectionsManagerImpl::OnConnectionRequested,
                     weak_ptr_factory_.GetWeakPtr(), endpoint_id,
                     std::move(callback)));
}

void NearbyConnectionsManagerImpl::OnConnectionRequested(
    const std::string& endpoint_id,
    NearbyConnectionCallback callback,
    ConnectionsStatus status) {
  if (status != ConnectionsStatus::kSuccess) {
    NS_LOG(ERROR) << "Failed to connect to the remote shareTarget: " << status;
    nearby_connections_->DisconnectFromEndpoint(endpoint_id, base::DoNothing());
    std::move(callback).Run(nullptr);
    return;
  }

  auto result =
      pending_outgoing_connections_.emplace(endpoint_id, std::move(callback));
  DCHECK(result.second);

  // TODO(crbug/1111458): Support TransferManager.
}

void NearbyConnectionsManagerImpl::Disconnect(const std::string& endpoint_id) {
  if (!nearby_connections_)
    return;

  nearby_connections_->DisconnectFromEndpoint(endpoint_id, base::DoNothing());
  OnDisconnected(endpoint_id);
  NS_LOG(INFO) << "Disconnected from " << endpoint_id;
}

void NearbyConnectionsManagerImpl::Send(const std::string& endpoint_id,
                                        PayloadPtr payload,
                                        PayloadStatusListener* listener,
                                        ConnectionsCallback callback) {
  if (!nearby_connections_) {
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  // TOOD(crbug/1076008): Implement.
}

void NearbyConnectionsManagerImpl::RegisterPayloadStatusListener(
    int64_t payload_id,
    PayloadStatusListener* listener) {
  if (!nearby_connections_)
    return;

  // TOOD(crbug/1076008): Implement.
}

NearbyConnectionsManagerImpl::Payload*
NearbyConnectionsManagerImpl::GetIncomingPayload(int64_t payload_id) {
  // TOOD(crbug/1076008): Implement.
  return nullptr;
}

void NearbyConnectionsManagerImpl::Cancel(int64_t payload_id,
                                          ConnectionsCallback callback) {
  if (!nearby_connections_) {
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  // TOOD(crbug/1076008): Implement.
}

void NearbyConnectionsManagerImpl::ClearIncomingPayloads() {
  // TOOD(crbug/1076008): Implement.
}

base::Optional<std::vector<uint8_t>>
NearbyConnectionsManagerImpl::GetRawAuthenticationToken(
    const std::string& endpoint_id) {
  auto it = connection_info_map_.find(endpoint_id);
  if (it == connection_info_map_.end())
    return base::nullopt;

  return it->second->raw_authentication_token;
}

void NearbyConnectionsManagerImpl::UpgradeBandwidth(
    const std::string& endpoint_id) {
  // TODO(crbug/1076008): Implement.
}

void NearbyConnectionsManagerImpl::OnNearbyProfileChanged(Profile* profile) {
  NS_LOG(VERBOSE) << __func__;
}

void NearbyConnectionsManagerImpl::OnNearbyProcessStarted() {
  NS_LOG(VERBOSE) << __func__;
}

void NearbyConnectionsManagerImpl::OnNearbyProcessStopped() {
  NS_LOG(VERBOSE) << __func__;
  Reset();
}

void NearbyConnectionsManagerImpl::OnEndpointFound(
    const std::string& endpoint_id,
    DiscoveredEndpointInfoPtr info) {
  if (!discovery_listener_) {
    NS_LOG(INFO) << "Ignoring discovered endpoint "
                 << base::HexEncode(info->endpoint_info.data(),
                                    info->endpoint_info.size())
                 << " because we're no longer "
                    "in discovery mode";
    return;
  }

  auto result = discovered_endpoints_.insert(endpoint_id);
  if (!result.second) {
    NS_LOG(INFO) << "Ignoring discovered endpoint "
                 << base::HexEncode(info->endpoint_info.data(),
                                    info->endpoint_info.size())
                 << " because we've already "
                    "reported this endpoint";
    return;
  }

  discovery_listener_->OnEndpointDiscovered(endpoint_id, info->endpoint_info);
  NS_LOG(INFO) << "Discovered "
               << base::HexEncode(info->endpoint_info.data(),
                                  info->endpoint_info.size())
               << " over Nearby Connections";
}

void NearbyConnectionsManagerImpl::OnEndpointLost(
    const std::string& endpoint_id) {
  if (!discovered_endpoints_.erase(endpoint_id)) {
    NS_LOG(INFO) << "Ignoring lost endpoint " << endpoint_id
                 << " because we haven't reported this endpoint";
    return;
  }

  if (!discovery_listener_) {
    NS_LOG(INFO) << "Ignoring lost endpoint " << endpoint_id
                 << " because we're no longer in discovery mode";
    return;
  }

  discovery_listener_->OnEndpointLost(endpoint_id);
  NS_LOG(INFO) << "Endpoint " << endpoint_id << " lost over Nearby Connections";
}

void NearbyConnectionsManagerImpl::OnConnectionInitiated(
    const std::string& endpoint_id,
    ConnectionInfoPtr info) {
  auto result = connection_info_map_.emplace(endpoint_id, std::move(info));
  DCHECK(result.second);
  // TOOD(crbug/1076008): Implemnet AcceptConnection.
  // nearby_connections_->AcceptConnection(
  //     endpoint_id, payload_listener_.BindNewPipeAndPassRemote());
}

void NearbyConnectionsManagerImpl::OnConnectionAccepted(
    const std::string& endpoint_id) {
  auto it = connection_info_map_.find(endpoint_id);
  if (it == connection_info_map_.end())
    return;

  if (it->second->is_incoming_connection) {
    // TOOD(crbug/1076008): Handle incoming connection.
  } else {
    auto it = pending_outgoing_connections_.find(endpoint_id);
    if (it == pending_outgoing_connections_.end()) {
      Disconnect(endpoint_id);
      return;
    }

    auto result = connections_.emplace(
        endpoint_id, std::make_unique<NearbyConnectionImpl>(this, endpoint_id));
    DCHECK(result.second);
    std::move(it->second).Run(result.first->second.get());
    pending_outgoing_connections_.erase(it);
  }
}

void NearbyConnectionsManagerImpl::OnConnectionRejected(
    const std::string& endpoint_id,
    Status status) {
  connection_info_map_.erase(endpoint_id);

  auto it = pending_outgoing_connections_.find(endpoint_id);
  if (it != pending_outgoing_connections_.end()) {
    std::move(it->second).Run(nullptr);
    pending_outgoing_connections_.erase(it);
  }

  // TODO(crbug/1111458): Support TransferManager.
}

void NearbyConnectionsManagerImpl::OnDisconnected(
    const std::string& endpoint_id) {
  connection_info_map_.erase(endpoint_id);

  auto it = pending_outgoing_connections_.find(endpoint_id);
  if (it != pending_outgoing_connections_.end()) {
    std::move(it->second).Run(nullptr);
    pending_outgoing_connections_.erase(it);
  }

  connections_.erase(endpoint_id);

  // TODO(crbug/1111458): Support TransferManager.
}

void NearbyConnectionsManagerImpl::OnBandwidthChanged(
    const std::string& endpoint_id,
    int32_t quality) {
  NS_LOG(VERBOSE) << __func__;
  // TODO(crbug/1111458): Support TransferManager.
}

bool NearbyConnectionsManagerImpl::BindNearbyConnections() {
  if (!nearby_connections_) {
    nearby_connections_ =
        process_manager_->GetOrStartNearbyConnections(profile_);
  }
  return nearby_connections_ != nullptr;
}

void NearbyConnectionsManagerImpl::Reset() {
  nearby_connections_ = nullptr;
  discovered_endpoints_.clear();
  discovery_listener_ = nullptr;
  endpoint_discovery_listener_.reset();
}
