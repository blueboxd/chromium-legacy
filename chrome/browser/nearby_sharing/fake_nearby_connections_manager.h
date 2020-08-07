// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"

// Fake NearbyConnectionsManager for testing.
class FakeNearbyConnectionsManager : public NearbyConnectionsManager {
 public:

  FakeNearbyConnectionsManager();
  ~FakeNearbyConnectionsManager() override;

  // NearbyConnectionsManager:
  void Shutdown() override;
  void StartAdvertising(std::vector<uint8_t> endpoint_info,
                        IncomingConnectionListener* listener,
                        PowerLevel power_level,
                        DataUsage data_usage,
                        ConnectionsCallback callback) override;
  void StopAdvertising() override;
  void StartDiscovery(DiscoveryListener* listener,
                      ConnectionsCallback callback) override;
  void StopDiscovery() override;
  void Connect(std::vector<uint8_t> endpoint_info,
               const std::string& endpoint_id,
               base::Optional<std::vector<uint8_t>> bluetooth_mac_address,
               DataUsage data_usage,
               NearbyConnectionCallback callback) override;
  void Disconnect(const std::string& endpoint_id) override;
  void Send(const std::string& endpoint_id,
            PayloadPtr payload,
            PayloadStatusListener* listener,
            ConnectionsCallback callback) override;
  void RegisterPayloadStatusListener(int64_t payload_id,
                                     PayloadStatusListener* listener) override;
  Payload* GetIncomingPayload(int64_t payload_id) override;
  void Cancel(int64_t payload_id, ConnectionsCallback callback) override;
  void ClearIncomingPayloads() override;
  base::Optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      const std::string& endpoint_id) override;
  void UpgradeBandwidth(const std::string& endpoint_id) override;

  // Testing methods
  bool IsAdvertising();
  bool IsDiscovering();
  bool IsShutdown();
  DataUsage GetAdvertisingDataUsage();
  PowerLevel GetAdvertisingPowerLevel();
  bool DidUpgradeBandwidth(const std::string& endpoint_id);

 private:
  IncomingConnectionListener* advertising_listener_ = nullptr;
  DiscoveryListener* discovery_listener_ = nullptr;
  bool is_shutdown_ = false;
  DataUsage advertising_data_usage_ = DataUsage::kUnknown;
  PowerLevel advertising_power_level_ = PowerLevel::kUnknown;
  std::set<std::string> upgrade_bandwidth_endpoint_ids_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_
