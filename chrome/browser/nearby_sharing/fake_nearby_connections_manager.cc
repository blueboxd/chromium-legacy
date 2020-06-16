// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fake_nearby_connections_manager.h"

FakeNearbyConnectionsManager::FakeNearbyConnection::FakeNearbyConnection() =
    default;

FakeNearbyConnectionsManager::FakeNearbyConnection::~FakeNearbyConnection() =
    default;

void FakeNearbyConnectionsManager::FakeNearbyConnection::Read(
    ReadCallback callback) {
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::FakeNearbyConnection::Write(
    std::vector<uint8_t> bytes,
    WriteCallback callback) {
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::FakeNearbyConnection::Close() {
  is_closed_ = true;
  if (!disconnection_callback_.is_null())
    std::move(disconnection_callback_).Run();
}

bool FakeNearbyConnectionsManager::FakeNearbyConnection::IsClosed() {
  return is_closed_;
}

void FakeNearbyConnectionsManager::FakeNearbyConnection::
    RegisterForDisconnection(base::OnceClosure callback) {
  disconnection_callback_ = std::move(callback);
}

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
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::StopAdvertising() {
  DCHECK(IsAdvertising());
  DCHECK(!IsShutdown());
  advertising_listener_ = nullptr;
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::StartDiscovery(
    std::vector<uint8_t> endpoint_info,
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

std::unique_ptr<NearbyConnection> FakeNearbyConnectionsManager::Connect(
    std::vector<uint8_t> endpoint_info,
    const std::string& endpoint_id,
    base::Optional<std::vector<uint8_t>> bluetooth_mac_address,
    DataUsage data_usage,
    ConnectionsCallback callback) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
  return std::make_unique<FakeNearbyConnection>();
}

void FakeNearbyConnectionsManager::Disconnect(const std::string& endpoint_id) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::Send(const std::string& endpoint_id,
                                        PayloadPtr payload,
                                        PayloadStatusListener* listener,
                                        ConnectionsCallback callback) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::RegisterPayloadStatusListener(
    int64_t payload_id,
    PayloadStatusListener* listener) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
}

FakeNearbyConnectionsManager::PayloadPtr
FakeNearbyConnectionsManager::GetIncomingPayload(int64_t payload_id) {
  DCHECK(!IsShutdown());
  // TODO(alexchau): Implement.
  return nullptr;
}

void FakeNearbyConnectionsManager::Cancel(int64_t payload_id,
                                          ConnectionsCallback callback) {
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
  // TODO(alexchau): Implement.
  return base::nullopt;
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
