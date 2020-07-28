// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_connections_manager_impl.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/services/sharing/public/mojom/nearby_connections_types.mojom.h"

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

std::unique_ptr<NearbyConnection> NearbyConnectionsManagerImpl::Connect(
    std::vector<uint8_t> endpoint_info,
    const std::string& endpoint_id,
    base::Optional<std::vector<uint8_t>> bluetooth_mac_address,
    DataUsage data_usage,
    ConnectionsCallback callback) {
  if (!nearby_connections_) {
    std::move(callback).Run(ConnectionsStatus::kError);
    return nullptr;
  }

  // TOOD(crbug/1076008): Implement.
  return nullptr;
}

void NearbyConnectionsManagerImpl::Disconnect(const std::string& endpoint_id) {
  if (!nearby_connections_)
    return;

  // TOOD(crbug/1076008): Implement.
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

NearbyConnectionsManagerImpl::PayloadPtr
NearbyConnectionsManagerImpl::GetIncomingPayload(int64_t payload_id) {
  if (!nearby_connections_)
    return nullptr;

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
  if (!nearby_connections_)
    return;

  // TOOD(crbug/1076008): Implement.
}

base::Optional<std::vector<uint8_t>>
NearbyConnectionsManagerImpl::GetRawAuthenticationToken(
    const std::string& endpoint_id) {
  if (!nearby_connections_)
    return base::nullopt;

  // TOOD(crbug/1076008): Implement.
  return base::nullopt;
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
