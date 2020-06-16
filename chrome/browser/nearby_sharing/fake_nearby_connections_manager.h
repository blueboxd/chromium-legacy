// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_

#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"

// Fake NearbyConnectionsManager for testing.
class FakeNearbyConnectionsManager : public NearbyConnectionsManager {
 public:
  class FakeNearbyConnection : public NearbyConnection {
   public:
    FakeNearbyConnection();
    ~FakeNearbyConnection() override;

    // NearbyConnection:
    void Read(ReadCallback callback) override;
    void Write(std::vector<uint8_t> bytes, WriteCallback callback) override;
    void Close() override;
    bool IsClosed() override;
    void RegisterForDisconnection(base::OnceClosure callback) override;

   private:
    bool is_closed_{false};
    base::OnceClosure disconnection_callback_;
  };

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
  void StartDiscovery(std::vector<uint8_t> endpoint_info,
                      DiscoveryListener* listener,
                      ConnectionsCallback callback) override;
  void StopDiscovery() override;
  std::unique_ptr<NearbyConnection> Connect(
      std::vector<uint8_t> endpoint_info,
      const std::string& endpoint_id,
      base::Optional<std::vector<uint8_t>> bluetooth_mac_address,
      DataUsage data_usage,
      ConnectionsCallback callback) override;
  void Disconnect(const std::string& endpoint_id) override;
  void Send(const std::string& endpoint_id,
            PayloadPtr payload,
            PayloadStatusListener* listener,
            ConnectionsCallback callback) override;
  void RegisterPayloadStatusListener(int64_t payload_id,
                                     PayloadStatusListener* listener) override;
  PayloadPtr GetIncomingPayload(int64_t payload_id) override;
  void Cancel(int64_t payload_id, ConnectionsCallback callback) override;
  void ClearIncomingPayloads() override;
  base::Optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      const std::string& endpoint_id) override;

  // Testing methods
  bool IsAdvertising();
  bool IsDiscovering();
  bool IsShutdown();

 private:
  IncomingConnectionListener* advertising_listener_{nullptr};
  DiscoveryListener* discovery_listener_{nullptr};
  bool is_shutdown_{false};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_
