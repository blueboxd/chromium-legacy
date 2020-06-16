// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_connections_manager_impl.h"

NearbyConnectionsManagerImpl::NearbyConnectionsManagerImpl() = default;

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
  // TOOD(crbug/1076008): Implement.
}

void NearbyConnectionsManagerImpl::StopAdvertising() {
  // TOOD(crbug/1076008): Implement.
}

void NearbyConnectionsManagerImpl::StartDiscovery(
    std::vector<uint8_t> endpoint_info,
    DiscoveryListener* listener,
    ConnectionsCallback callback) {
  // TOOD(crbug/1076008): Implement.
}

void NearbyConnectionsManagerImpl::StopDiscovery() {
  // TOOD(crbug/1076008): Implement.
}

std::unique_ptr<NearbyConnection> NearbyConnectionsManagerImpl::Connect(
    std::vector<uint8_t> endpoint_info,
    const std::string& endpoint_id,
    base::Optional<std::vector<uint8_t>> bluetooth_mac_address,
    DataUsage data_usage,
    ConnectionsCallback callback) {
  // TOOD(crbug/1076008): Implement.
  return nullptr;
}

void NearbyConnectionsManagerImpl::Disconnect(const std::string& endpoint_id) {
  // TOOD(crbug/1076008): Implement.
}

void NearbyConnectionsManagerImpl::Send(const std::string& endpoint_id,
                                        PayloadPtr payload,
                                        PayloadStatusListener* listener,
                                        ConnectionsCallback callback) {
  // TOOD(crbug/1076008): Implement.
}

void NearbyConnectionsManagerImpl::RegisterPayloadStatusListener(
    int64_t payload_id,
    PayloadStatusListener* listener) {
  // TOOD(crbug/1076008): Implement.
}

NearbyConnectionsManagerImpl::PayloadPtr
NearbyConnectionsManagerImpl::GetIncomingPayload(int64_t payload_id) {
  // TOOD(crbug/1076008): Implement.
  return nullptr;
}

void NearbyConnectionsManagerImpl::Cancel(int64_t payload_id,
                                          ConnectionsCallback callback) {
  // TOOD(crbug/1076008): Implement.
}

void NearbyConnectionsManagerImpl::ClearIncomingPayloads() {
  // TOOD(crbug/1076008): Implement.
}

base::Optional<std::vector<uint8_t>>
NearbyConnectionsManagerImpl::GetRawAuthenticationToken(
    const std::string& endpoint_id) {
  // TOOD(crbug/1076008): Implement.
  return base::nullopt;
}
