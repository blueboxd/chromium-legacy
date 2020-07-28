// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/test_support/mock_bluetooth_adapter.h"
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
const char kRemoteEndpointInfo[] = {0x0d, 0x07, 0x06, 0x08, 0x09};

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

class NearbyConnectionsTest : public testing::Test {
 public:
  NearbyConnectionsTest() {
    auto webrtc_dependencies = mojom::WebRtcDependencies::New(
        webrtc_dependencies_.socket_manager_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.mdns_responder_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.ice_config_fetcher_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.messenger_.BindNewPipeAndPassRemote());
    auto dependencies = mojom::NearbyConnectionsDependencies::New(
        bluetooth_adapter_.adapter.BindNewPipeAndPassRemote(),
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
  bluetooth::MockBluetoothAdapter bluetooth_adapter_;
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
  bluetooth_adapter_.adapter.reset();
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

}  // namespace connections
}  // namespace nearby
}  // namespace location
