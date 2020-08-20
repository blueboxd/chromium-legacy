// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fake_nearby_connections_manager.h"

FakeNearbyConnectionsManager::FakeNearbyConnectionsManager() = default;

FakeNearbyConnectionsManager::~FakeNearbyConnectionsManager() = default;

void FakeNearbyConnectionsManager::Shutdown() {
  DCHECK(!IsAdvertising());
  DCHECK(!IsDiscovering());
  is_shutdown_ = true;
}

void FakeNearbyConnectionsManager::StartAdvertising(
    std::vector<uint8_t> endpoint_info,
    IncomingConnectionListener* listener,
    PowerLevel power_level,
    DataUsage data_usage,
    ConnectionsCallback callback) {
  is_shutdown_ = false;
  advertising_listener_ = listener;
  advertising_data_usage_ = data_usage;
  advertising_power_level_ = power_level;
}

void FakeNearbyConnectionsManager::StopAdvertising() {
  DCHECK(IsAdvertising());
  DCHECK(!IsShutdown());
  advertising_listener_ = nullptr;
  advertising_data_usage_ = DataUsage::kUnknown;
  advertising_power_level_ = PowerLevel::kUnknown;
}

void FakeNearbyConnectionsManager::StartDiscovery(
    DiscoveryListener* listener,
    ConnectionsCallback callback) {
  is_shutdown_ = false;
  discovery_listener_ = listener;
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::StopDiscovery() {
  DCHECK(IsDiscovering());
  DCHECK(!IsShutdown());
  discovery_listener_ = nullptr;
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::Connect(
    std::vector<uint8_t> endpoint_info,
    const std::string& endpoint_id,
    base::Optional<std::vector<uint8_t>> bluetooth_mac_address,
    DataUsage data_usage,
    NearbyConnectionCallback callback) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::Disconnect(const std::string& endpoint_id) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::Send(const std::string& endpoint_id,
                                        PayloadPtr payload,
                                        PayloadStatusListener* listener) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::RegisterPayloadStatusListener(
    int64_t payload_id,
    PayloadStatusListener* listener) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::RegisterPayloadPath(
    int64_t payload_id,
    const base::FilePath& file_path,
    ConnectionsCallback callback) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
}

FakeNearbyConnectionsManager::Payload*
FakeNearbyConnectionsManager::GetIncomingPayload(int64_t payload_id) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
  return nullptr;
}

void FakeNearbyConnectionsManager::Cancel(int64_t payload_id) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::ClearIncomingPayloads() {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
}

base::Optional<std::vector<uint8_t>>
FakeNearbyConnectionsManager::GetRawAuthenticationToken(
    const std::string& endpoint_id) {
  DCHECK(!IsShutdown());

  auto iter = endpoint_auth_tokens_.find(endpoint_id);
  if (iter != endpoint_auth_tokens_.end())
    return iter->second;

  return base::nullopt;
}

void FakeNearbyConnectionsManager::SetRawAuthenticationToken(
    const std::string& endpoint_id,
    std::vector<uint8_t> token) {
  endpoint_auth_tokens_[endpoint_id] = std::move(token);
}

void FakeNearbyConnectionsManager::UpgradeBandwidth(
    const std::string& endpoint_id) {
  upgrade_bandwidth_endpoint_ids_.insert(endpoint_id);
}

void FakeNearbyConnectionsManager::OnEndpointFound(
    const std::string& endpoint_id,
    location::nearby::connections::mojom::DiscoveredEndpointInfoPtr info) {
  if (!discovery_listener_)
    return;

  discovery_listener_->OnEndpointDiscovered(endpoint_id, info->endpoint_info);
}

void FakeNearbyConnectionsManager::OnEndpointLost(
    const std::string& endpoint_id) {
  if (!discovery_listener_)
    return;

  discovery_listener_->OnEndpointLost(endpoint_id);
}

bool FakeNearbyConnectionsManager::IsAdvertising() {
  return advertising_listener_ != nullptr;
}

bool FakeNearbyConnectionsManager::IsDiscovering() {
  return discovery_listener_ != nullptr;
}

bool FakeNearbyConnectionsManager::IsShutdown() {
  return is_shutdown_;
}

DataUsage FakeNearbyConnectionsManager::GetAdvertisingDataUsage() {
  return advertising_data_usage_;
}

PowerLevel FakeNearbyConnectionsManager::GetAdvertisingPowerLevel() {
  return advertising_power_level_;
}

bool FakeNearbyConnectionsManager::DidUpgradeBandwidth(
    const std::string& endpoint_id) {
  return (upgrade_bandwidth_endpoint_ids_.count(endpoint_id) > 0);
}
