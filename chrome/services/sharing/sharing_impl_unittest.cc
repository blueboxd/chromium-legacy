// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/sharing_impl.h"

#include "base/test/task_environment.h"
#include "chrome/services/sharing/webrtc/test/mock_sharing_connection_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sharing {

class SharingImplTest : public testing::Test {
 public:
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

}  // namespace sharing
