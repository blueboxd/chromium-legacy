// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/sharing_impl.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/nearby_connections.h"
#include "chrome/services/sharing/nearby/test_support/mock_bluetooth_adapter.h"
#include "chrome/services/sharing/nearby/test_support/mock_webrtc_signaling_messenger.h"
#include "chrome/services/sharing/public/mojom/nearby_decoder.mojom.h"
#include "chrome/services/sharing/webrtc/test/mock_sharing_connection_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sharing {

class SharingImplTest : public testing::Test {
 public:
  using NearbyConnectionsMojom =
      location::nearby::connections::mojom::NearbyConnections;
  using NearbySharingDecoderMojom = sharing::mojom::NearbySharingDecoder;
  using NearbyConnections = location::nearby::connections::NearbyConnections;

  SharingImplTest() {
    service_ =
        std::make_unique<SharingImpl>(remote_.BindNewPipeAndPassReceiver());
  }

  ~SharingImplTest() override {
    // Let libjingle threads finish.
    base::RunLoop().RunUntilIdle();
  }

  SharingImpl* service() const { return service_.get(); }

  std::unique_ptr<MockSharingConnectionHost> CreateWebRtcConnection() {
    auto connection = std::make_unique<MockSharingConnectionHost>();
    service_->CreateSharingWebRtcConnection(
        connection->signalling_sender.BindNewPipeAndPassRemote(),
        connection->signalling_receiver.BindNewPipeAndPassReceiver(),
        connection->delegate.BindNewPipeAndPassRemote(),
        connection->connection.BindNewPipeAndPassReceiver(),
        connection->socket_manager.BindNewPipeAndPassRemote(),
        connection->mdns_responder.BindNewPipeAndPassRemote(),
        /*ice_servers=*/{});
    return connection;
  }

  mojo::Remote<NearbyConnectionsMojom> CreateNearbyConnections(
      mojo::PendingRemote<bluetooth::mojom::Adapter> bluetooth_adapter,
      mojo::PendingRemote<sharing::mojom::WebRtcSignalingMessenger>
          webrtc_signaling_messenger) {
    mojo::Remote<NearbyConnectionsMojom> connections;
    auto dependencies =
        location::nearby::connections::mojom::NearbyConnectionsDependencies::
            New(std::move(bluetooth_adapter),
                std::move(webrtc_signaling_messenger));
    base::RunLoop run_loop;
    service()->CreateNearbyConnections(
        std::move(dependencies),
        base::BindLambdaForTesting(
            [&](mojo::PendingRemote<NearbyConnectionsMojom> pending_remote) {
              connections.Bind(std::move(pending_remote));
              run_loop.Quit();
            }));
    // Wait until NearbyConnectionsMojom is connected.
    run_loop.Run();
    return connections;
  }

  mojo::Remote<NearbySharingDecoderMojom> CreateNearbySharingDecoder() {
    mojo::Remote<NearbySharingDecoderMojom> remote;
    base::RunLoop run_loop;
    service()->CreateNearbySharingDecoder(base::BindLambdaForTesting(
        [&](mojo::PendingRemote<NearbySharingDecoderMojom> pending_remote) {
          remote.Bind(std::move(pending_remote));
          run_loop.Quit();
        }));
    // Wait until NearbySharingDecoderMojom is connected.
    run_loop.Run();
    return remote;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::Sharing> remote_;
  std::unique_ptr<SharingImpl> service_;
};

TEST_F(SharingImplTest, CreatesSinglePeerConnection) {
  auto connection = CreateWebRtcConnection();

  EXPECT_EQ(1u, service()->GetWebRtcConnectionCountForTesting());
}

TEST_F(SharingImplTest, CreatesMultiplePeerConnections) {
  auto connection1 = CreateWebRtcConnection();
  auto connection2 = CreateWebRtcConnection();

  EXPECT_EQ(2u, service()->GetWebRtcConnectionCountForTesting());
}

TEST_F(SharingImplTest, ClosesPeerConnection) {
  auto connection1 = CreateWebRtcConnection();
  auto connection2 = CreateWebRtcConnection();

  {
    auto connection3 = CreateWebRtcConnection();
    EXPECT_EQ(3u, service()->GetWebRtcConnectionCountForTesting());
  }

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, service()->GetWebRtcConnectionCountForTesting());
}

TEST_F(SharingImplTest, NearbyConnections_Create) {
  bluetooth::MockBluetoothAdapter bluetooth_adapter;
  sharing::MockWebRtcSignalingMessenger webrtc_signaling_messenger;
  mojo::Remote<NearbyConnectionsMojom> connections = CreateNearbyConnections(
      bluetooth_adapter.adapter.BindNewPipeAndPassRemote(),
      webrtc_signaling_messenger.messenger.BindNewPipeAndPassRemote());

  EXPECT_TRUE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_CreateMultiple) {
  bluetooth::MockBluetoothAdapter bluetooth_adapter_1;
  sharing::MockWebRtcSignalingMessenger webrtc_signaling_messenger_1;
  mojo::Remote<NearbyConnectionsMojom> connections_1 = CreateNearbyConnections(
      bluetooth_adapter_1.adapter.BindNewPipeAndPassRemote(),
      webrtc_signaling_messenger_1.messenger.BindNewPipeAndPassRemote());
  EXPECT_TRUE(connections_1.is_connected());

  // Calling CreateNearbyConnections() again should disconnect the old instance.
  bluetooth::MockBluetoothAdapter bluetooth_adapter_2;
  sharing::MockWebRtcSignalingMessenger webrtc_signaling_messenger_2;
  mojo::Remote<NearbyConnectionsMojom> connections_2 = CreateNearbyConnections(
      bluetooth_adapter_2.adapter.BindNewPipeAndPassRemote(),
      webrtc_signaling_messenger_2.messenger.BindNewPipeAndPassRemote());

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections_1.is_connected());
  EXPECT_TRUE(connections_2.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_BluetoothDisconnects) {
  bluetooth::MockBluetoothAdapter bluetooth_adapter;
  sharing::MockWebRtcSignalingMessenger webrtc_signaling_messenger;
  mojo::Remote<NearbyConnectionsMojom> connections = CreateNearbyConnections(
      bluetooth_adapter.adapter.BindNewPipeAndPassRemote(),
      webrtc_signaling_messenger.messenger.BindNewPipeAndPassRemote());
  EXPECT_TRUE(connections.is_connected());

  // Disconnecting the |bluetooth_adapter| interface should also
  // disconnect and destroy the |connections| interface.
  bluetooth_adapter.adapter.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcSignalingMessengerDisconnects) {
  bluetooth::MockBluetoothAdapter bluetooth_adapter;
  sharing::MockWebRtcSignalingMessenger webrtc_signaling_messenger;
  mojo::Remote<NearbyConnectionsMojom> connections = CreateNearbyConnections(
      bluetooth_adapter.adapter.BindNewPipeAndPassRemote(),
      webrtc_signaling_messenger.messenger.BindNewPipeAndPassRemote());
  EXPECT_TRUE(connections.is_connected());

  // Disconnecting the |webrtc_signaling_messenger| interface should also
  // disconnect and destroy the |connections| interface.
  webrtc_signaling_messenger.messenger.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_NullBluetoothAdapter) {
  sharing::MockWebRtcSignalingMessenger webrtc_signaling_messenger;
  mojo::Remote<NearbyConnectionsMojom> connections = CreateNearbyConnections(
      mojo::NullRemote(),
      webrtc_signaling_messenger.messenger.BindNewPipeAndPassRemote());
  EXPECT_TRUE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbySharingDecoder_Create) {
  mojo::Remote<NearbySharingDecoderMojom> remote = CreateNearbySharingDecoder();
  EXPECT_TRUE(remote.is_connected());
}

TEST_F(SharingImplTest, NearbySharingDecoder_CreateMultiple) {
  mojo::Remote<NearbySharingDecoderMojom> remote_1 =
      CreateNearbySharingDecoder();
  EXPECT_TRUE(remote_1.is_connected());

  // Calling CreateNearbySharingDecoder() again should disconnect the old
  // instance.
  mojo::Remote<NearbySharingDecoderMojom> remote_2 =
      CreateNearbySharingDecoder();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(remote_1.is_connected());
  EXPECT_TRUE(remote_2.is_connected());
}

}  // namespace sharing
