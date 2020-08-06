// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "chrome/services/sharing/nearby/test_support/mock_webrtc_dependencies.h"
#include "chrome/services/sharing/public/mojom/nearby_decoder.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/src/cpp/core_v2/internal/mock_service_controller.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

const char kServiceId[] = "service-id";
const char kRemoteEndpointId[] = "remote_endpoint_id";
const char kEndpointInfo[] = {0x0d, 0x07, 0x07, 0x07, 0x07};
const char kRemoteEndpointInfo[] = {0x0d, 0x07, 0x06, 0x08, 0x09};
const char kAuthenticationToken[] = "authentication_token";
const char kRawAuthenticationToken[] = {0x00, 0x05, 0x04, 0x03, 0x02};
const int32_t kQuality = 5201314;

}  // namespace

class FakeEndpointDiscoveryListener : public mojom::EndpointDiscoveryListener {
 public:
  void OnEndpointFound(const std::string& endpoint_id,
                       mojom::DiscoveredEndpointInfoPtr info) override {
    endpoint_found_cb.Run(endpoint_id, std::move(info));
  }

  void OnEndpointLost(const std::string& endpoint_id) override {
    endpoint_lost_cb.Run(endpoint_id);
  }

  mojo::Receiver<mojom::EndpointDiscoveryListener> receiver{this};
  base::RepeatingCallback<void(const std::string&,
                               mojom::DiscoveredEndpointInfoPtr)>
      endpoint_found_cb = base::DoNothing();
  base::RepeatingCallback<void(const std::string&)> endpoint_lost_cb =
      base::DoNothing();
};

class FakeConnectionLifecycleListener
    : public mojom::ConnectionLifecycleListener {
 public:
  void OnConnectionInitiated(const std::string& endpoint_id,
                             mojom::ConnectionInfoPtr info) override {
    initiated_cb.Run(endpoint_id, std::move(info));
  }

  void OnConnectionAccepted(const std::string& endpoint_id) override {
    accepted_cb.Run(endpoint_id);
  }

  void OnConnectionRejected(const std::string& endpoint_id,
                            mojom::Status status) override {
    rejected_cb.Run(endpoint_id, status);
  }

  void OnDisconnected(const std::string& endpoint_id) override {
    disconnected_cb.Run(endpoint_id);
  }

  void OnBandwidthChanged(const std::string& endpoint_id,
                          int32_t quality) override {
    bandwidth_changed_cb.Run(endpoint_id, quality);
  }

  mojo::Receiver<mojom::ConnectionLifecycleListener> receiver{this};
  base::RepeatingCallback<void(const std::string&, mojom::ConnectionInfoPtr)>
      initiated_cb = base::DoNothing();
  base::RepeatingCallback<void(const std::string&)> accepted_cb =
      base::DoNothing();
  base::RepeatingCallback<void(const std::string&, mojom::Status)> rejected_cb =
      base::DoNothing();
  base::RepeatingCallback<void(const std::string&)> disconnected_cb =
      base::DoNothing();
  base::RepeatingCallback<void(const std::string&, int32_t)>
      bandwidth_changed_cb = base::DoNothing();
};

class NearbyConnectionsTest : public testing::Test {
 public:
  NearbyConnectionsTest() {
    auto webrtc_dependencies = mojom::WebRtcDependencies::New(
        webrtc_dependencies_.socket_manager_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.mdns_responder_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.ice_config_fetcher_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.messenger_.BindNewPipeAndPassRemote());
    auto dependencies = mojom::NearbyConnectionsDependencies::New(
        bluetooth_adapter_.adapter_.BindNewPipeAndPassRemote(),
        std::move(webrtc_dependencies));
    service_controller_ =
        std::make_unique<testing::NiceMock<MockServiceController>>();
    service_controller_ptr_ = service_controller_.get();
    nearby_connections_ = std::make_unique<NearbyConnections>(
        remote_.BindNewPipeAndPassReceiver(), std::move(dependencies),
        base::BindOnce(&NearbyConnectionsTest::OnDisconnect,
                       base::Unretained(this)),
        std::make_unique<Core>(
            [&]() { return service_controller_.release(); }));
  }

  void OnDisconnect() { disconnect_run_loop_.Quit(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::NearbyConnections> remote_;
  bluetooth::FakeAdapter bluetooth_adapter_;
  sharing::MockWebRtcDependencies webrtc_dependencies_;
  std::unique_ptr<NearbyConnections> nearby_connections_;
  testing::NiceMock<MockServiceController>* service_controller_ptr_;
  base::RunLoop disconnect_run_loop_;

 private:
  std::unique_ptr<testing::NiceMock<MockServiceController>> service_controller_;
};

TEST_F(NearbyConnectionsTest, RemoteDisconnect) {
  remote_.reset();
  disconnect_run_loop_.Run();
}

TEST_F(NearbyConnectionsTest, BluetoothDisconnect) {
  bluetooth_adapter_.adapter_.reset();
  disconnect_run_loop_.Run();
}

TEST_F(NearbyConnectionsTest, P2PSocketManagerDisconnect) {
  webrtc_dependencies_.socket_manager_.reset();
  disconnect_run_loop_.Run();
}

TEST_F(NearbyConnectionsTest, MdnsResponderDisconnect) {
  webrtc_dependencies_.mdns_responder_.reset();
  disconnect_run_loop_.Run();
}

TEST_F(NearbyConnectionsTest, IceConfigFetcherDisconnect) {
  webrtc_dependencies_.ice_config_fetcher_.reset();
  disconnect_run_loop_.Run();
}

TEST_F(NearbyConnectionsTest, WebRtcSignalingMessengerDisconnect) {
  webrtc_dependencies_.messenger_.reset();
  disconnect_run_loop_.Run();
}

TEST_F(NearbyConnectionsTest, StartStopDiscovery) {
  ClientProxy* client_proxy;
  EXPECT_CALL(*service_controller_ptr_, StartDiscovery)
      .WillOnce([&client_proxy](ClientProxy* client,
                                const std::string& service_id,
                                const ConnectionOptions& options,
                                const DiscoveryListener& listener) {
        client_proxy = client;
        EXPECT_EQ(kServiceId, service_id);
        EXPECT_EQ(Strategy::kP2pPointToPoint, options.strategy);

        client->StartedDiscovery(service_id, options.strategy, listener,
                                 /*mediums=*/{});
        return Status{Status::kAlreadyDiscovering};
      });

  base::RunLoop start_discovery_run_loop;
  FakeEndpointDiscoveryListener fake_discovery_listener;
  nearby_connections_->StartDiscovery(
      kServiceId,
      mojom::DiscoveryOptions::New(mojom::Strategy::kP2pPointToPoint),
      fake_discovery_listener.receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kAlreadyDiscovering, status);
        start_discovery_run_loop.Quit();
      }));
  start_discovery_run_loop.Run();

  base::RunLoop endpoint_found_run_loop;
  fake_discovery_listener.endpoint_found_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id,
                                     mojom::DiscoveredEndpointInfoPtr info) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        EXPECT_EQ(std::vector<uint8_t>(std::begin(kRemoteEndpointInfo),
                                       std::end(kRemoteEndpointInfo)),
                  info->endpoint_info);
        EXPECT_EQ(kServiceId, info->service_id);
        endpoint_found_run_loop.Quit();
      });

  client_proxy->OnEndpointFound(kServiceId, kRemoteEndpointId,
                                std::string(std::begin(kRemoteEndpointInfo),
                                            std::end(kRemoteEndpointInfo)),
                                /*mediums=*/{});
  endpoint_found_run_loop.Run();

  base::RunLoop endpoint_lost_run_loop;
  fake_discovery_listener.endpoint_lost_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        endpoint_lost_run_loop.Quit();
      });
  client_proxy->OnEndpointLost(kServiceId, kRemoteEndpointId);
  endpoint_lost_run_loop.Run();

  EXPECT_CALL(*service_controller_ptr_, StopDiscovery(testing::_)).Times(1);

  base::RunLoop stop_discovery_run_loop;
  nearby_connections_->StopDiscovery(
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        stop_discovery_run_loop.Quit();
      }));
  stop_discovery_run_loop.Run();

  // StopDiscovery is also called when Core is destroyed.
  EXPECT_CALL(*service_controller_ptr_, StopDiscovery(testing::_)).Times(1);
}

TEST_F(NearbyConnectionsTest, RequestConnection) {
  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;

  EXPECT_CALL(*service_controller_ptr_, StartDiscovery)
      .WillOnce([](ClientProxy* client, const std::string& service_id,
                   const ConnectionOptions& options,
                   const DiscoveryListener& listener) {
        client->StartedDiscovery(service_id, options.strategy, listener,
                                 /*mediums=*/{});
        client->OnEndpointFound(kServiceId, kRemoteEndpointId,
                                std::string(std::begin(kRemoteEndpointInfo),
                                            std::end(kRemoteEndpointInfo)),
                                /*mediums=*/{});
        return Status{Status::kSuccess};
      });

  base::RunLoop start_discovery_run_loop;
  FakeEndpointDiscoveryListener fake_discovery_listener;
  nearby_connections_->StartDiscovery(
      kServiceId,
      mojom::DiscoveryOptions::New(mojom::Strategy::kP2pPointToPoint),
      fake_discovery_listener.receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        start_discovery_run_loop.Quit();
      }));
  start_discovery_run_loop.Run();

  ClientProxy* client_proxy;
  ConnectionListener connections_listener;
  EXPECT_CALL(*service_controller_ptr_, RequestConnection)
      .WillOnce([&client_proxy, &connections_listener](
                    ClientProxy* client, const std::string& endpoint_id,
                    const ConnectionRequestInfo& info) {
        client_proxy = client;
        connections_listener = info.listener;
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        EXPECT_EQ(
            std::string(std::begin(kEndpointInfo), std::end(kEndpointInfo)),
            info.name);
        return Status{Status::kSuccess};
      });

  base::RunLoop request_connection_run_loop;
  nearby_connections_->RequestConnection(
      std::vector<uint8_t>(std::begin(kEndpointInfo), std::end(kEndpointInfo)),
      kRemoteEndpointId,
      fake_connection_life_cycle_listener.receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        request_connection_run_loop.Quit();
      }));
  request_connection_run_loop.Run();

  base::RunLoop initiated_run_loop;
  fake_connection_life_cycle_listener.initiated_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::ConnectionInfoPtr info) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        EXPECT_EQ(kAuthenticationToken, info->authentication_token);
        EXPECT_EQ(std::vector<uint8_t>(std::begin(kRawAuthenticationToken),
                                       std::end(kRawAuthenticationToken)),
                  info->raw_authentication_token);
        EXPECT_EQ(std::vector<uint8_t>(std::begin(kRemoteEndpointInfo),
                                       std::end(kRemoteEndpointInfo)),
                  info->endpoint_info);
        EXPECT_FALSE(info->is_incoming_connection);
        initiated_run_loop.Quit();
      });
  client_proxy->OnConnectionInitiated(
      kRemoteEndpointId,
      {.authentication_token = kAuthenticationToken,
       .raw_authentication_token =
           ByteArray(kRawAuthenticationToken, sizeof(kRawAuthenticationToken)),
       .endpoint_info =
           ByteArray(kRemoteEndpointInfo, sizeof(kRemoteEndpointInfo)),
       .is_incoming_connection = false},
      connections_listener);
  initiated_run_loop.Run();

  base::RunLoop rejected_run_loop;
  fake_connection_life_cycle_listener.rejected_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::Status status) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        EXPECT_EQ(mojom::Status::kConnectionRejected, status);
        rejected_run_loop.Quit();
      });
  client_proxy->OnConnectionRejected(kRemoteEndpointId,
                                     {Status::kConnectionRejected});
  rejected_run_loop.Run();

  // Initiate connection again to test accepted flow.
  base::RunLoop initiated_run_loop_2;
  fake_connection_life_cycle_listener.initiated_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::ConnectionInfoPtr info) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        EXPECT_FALSE(info->is_incoming_connection);
        initiated_run_loop_2.Quit();
      });
  client_proxy->OnConnectionInitiated(kRemoteEndpointId,
                                      {.is_incoming_connection = false},
                                      connections_listener);
  initiated_run_loop_2.Run();

  base::RunLoop accepted_run_loop;
  fake_connection_life_cycle_listener.accepted_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        accepted_run_loop.Quit();
      });
  client_proxy->OnConnectionAccepted(kRemoteEndpointId);
  accepted_run_loop.Run();

  base::RunLoop bandwidth_changed_run_loop;
  fake_connection_life_cycle_listener.bandwidth_changed_cb =
      base::BindLambdaForTesting(
          [&](const std::string& endpoint_id, int32_t quality) {
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            EXPECT_EQ(kQuality, quality);
            bandwidth_changed_run_loop.Quit();
          });
  client_proxy->OnBandwidthChanged(kRemoteEndpointId, kQuality);
  bandwidth_changed_run_loop.Run();

  base::RunLoop disconnected_run_loop;
  fake_connection_life_cycle_listener.disconnected_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        disconnected_run_loop.Quit();
      });
  client_proxy->OnDisconnected(kRemoteEndpointId, /*notify=*/true);
  disconnected_run_loop.Run();

  // Initiate and accept connection again to test DisconnectFromEndpoint.
  base::RunLoop initiated_run_loop_3;
  fake_connection_life_cycle_listener.initiated_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::ConnectionInfoPtr info) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        EXPECT_FALSE(info->is_incoming_connection);
        initiated_run_loop_3.Quit();
      });
  client_proxy->OnConnectionInitiated(kRemoteEndpointId,
                                      {.is_incoming_connection = false},
                                      connections_listener);
  initiated_run_loop_3.Run();

  base::RunLoop accepted_run_loop_2;
  fake_connection_life_cycle_listener.accepted_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        accepted_run_loop_2.Quit();
      });
  client_proxy->OnConnectionAccepted(kRemoteEndpointId);
  accepted_run_loop_2.Run();

  EXPECT_CALL(*service_controller_ptr_, DisconnectFromEndpoint)
      .WillOnce([](ClientProxy* client, const std::string& endpoint_id) {
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        return Status{Status::kSuccess};
      });

  base::RunLoop disconnect_from_endpoint_run_loop;
  nearby_connections_->DisconnectFromEndpoint(
      kRemoteEndpointId, base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        disconnect_from_endpoint_run_loop.Quit();
      }));
  disconnect_from_endpoint_run_loop.Run();

  // DisconnectFromEndpoint is also called when Core is destroyed.
  EXPECT_CALL(*service_controller_ptr_, DisconnectFromEndpoint).Times(1);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
