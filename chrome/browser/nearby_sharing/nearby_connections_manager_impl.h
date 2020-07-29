// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_

#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"

#include <set>

#include "base/gtest_prod_util.h"
#include "chrome/browser/nearby_sharing/nearby_process_manager.h"
#include "chrome/services/sharing/public/mojom/nearby_connections.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// Concrete NearbyConnectionsManager implementation.
class NearbyConnectionsManagerImpl
    : public NearbyConnectionsManager,
      public NearbyProcessManager::Observer,
      public location::nearby::connections::mojom::EndpointDiscoveryListener {
 public:
  NearbyConnectionsManagerImpl(NearbyProcessManager* process_manager,
                               Profile* profile);
  ~NearbyConnectionsManagerImpl() override;
  NearbyConnectionsManagerImpl(const NearbyConnectionsManagerImpl&) = delete;
  NearbyConnectionsManagerImpl& operator=(const NearbyConnectionsManagerImpl&) =
      delete;

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
  PayloadPtr GetIncomingPayload(int64_t payload_id) override;
  void Cancel(int64_t payload_id, ConnectionsCallback callback) override;
  void ClearIncomingPayloads() override;
  base::Optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      const std::string& endpoint_id) override;

 private:
  using DiscoveryOptions =
      location::nearby::connections::mojom::DiscoveryOptions;
  using EndpointDiscoveryListener =
      location::nearby::connections::mojom::EndpointDiscoveryListener;
  using DiscoveredEndpointInfoPtr =
      location::nearby::connections::mojom::DiscoveredEndpointInfoPtr;
  FRIEND_TEST_ALL_PREFIXES(NearbyConnectionsManagerImplTest,
                           DiscoveryProcessStopped);

  // NearbyProcessManager::Observer:
  void OnNearbyProfileChanged(Profile* profile) override;
  void OnNearbyProcessStarted() override;
  void OnNearbyProcessStopped() override;

  // mojom::EndpointDiscoveryListener:
  void OnEndpointFound(const std::string& endpoint_id,
                       DiscoveredEndpointInfoPtr info) override;
  void OnEndpointLost(const std::string& endpoint_id) override;

  bool BindNearbyConnections();
  void Reset();

  NearbyProcessManager* process_manager_;
  Profile* profile_;
  DiscoveryListener* discovery_listener_ = nullptr;
  std::set<std::string> discovered_endpoints_;

  ScopedObserver<NearbyProcessManager, NearbyProcessManager::Observer>
      nearby_process_observer_{this};
  mojo::Receiver<EndpointDiscoveryListener> endpoint_discovery_listener_{this};

  location::nearby::connections::mojom::NearbyConnections* nearby_connections_ =
      nullptr;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_
