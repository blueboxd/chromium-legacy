// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/sharing_webrtc_connection.h"

#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/webrtc/test/fake_port_allocator_factory.h"
#include "chrome/services/sharing/webrtc/test/mock_sharing_connection_host.h"
#include "jingle/glue/thread_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/test/mock_peerconnectioninterface.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace {

class SharingClient {
 public:
  SharingClient(
      webrtc::PeerConnectionFactoryInterface* pc_factory,
      std::unique_ptr<cricket::PortAllocator> port_allocator,
      base::OnceCallback<void(sharing::SharingWebRtcConnection*)> on_disconnect)
      : connection_(pc_factory,
                    /*ice_servers=*/{},
                    host_.signalling_sender.BindNewPipeAndPassRemote(),
                    host_.signalling_receiver.BindNewPipeAndPassReceiver(),
                    host_.delegate.BindNewPipeAndPassRemote(),
                    host_.connection.BindNewPipeAndPassReceiver(),
                    host_.socket_manager.BindNewPipeAndPassRemote(),
                    host_.mdns_responder.BindNewPipeAndPassRemote(),
                    std::move(port_allocator),
                    std::move(on_disconnect)) {}

  sharing::SharingWebRtcConnection& connection() { return connection_; }

  sharing::MockSharingConnectionHost& host() { return host_; }

  void ConnectTo(SharingClient* client) {
    EXPECT_CALL(host_, SendOffer(testing::_, testing::_))
        .Times(testing::AtLeast(0))
        .WillRepeatedly(testing::Invoke(
            &client->connection_,
            &sharing::SharingWebRtcConnection::OnOfferReceived));

    EXPECT_CALL(host_, SendIceCandidates(testing::_))
        .Times(testing::AtLeast(0))
        .WillRepeatedly(testing::Invoke(
            &client->connection_,
            &sharing::SharingWebRtcConnection::OnIceCandidatesReceived));
  }

 private:
  sharing::MockSharingConnectionHost host_;
  sharing::SharingWebRtcConnection connection_;
};

}  // namespace

namespace sharing {

class SharingWebRtcConnectionIntegrationTest : public testing::Test {
 public:
  SharingWebRtcConnectionIntegrationTest() {
    jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
    jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);

    webrtc::PeerConnectionFactoryDependencies dependencies;
    dependencies.task_queue_factory = CreateWebRtcTaskQueueFactory();
    dependencies.network_thread = rtc::Thread::Current();
    dependencies.worker_thread = rtc::Thread::Current();
    dependencies.signaling_thread = rtc::Thread::Current();

    webrtc_pc_factory_ =
        webrtc::CreateModularPeerConnectionFactory(std::move(dependencies));
  }

  ~SharingWebRtcConnectionIntegrationTest() override {
    // Let libjingle threads finish.
    base::RunLoop().RunUntilIdle();
  }

  void ConnectionClosed(SharingWebRtcConnection* connection) {}

  std::unique_ptr<SharingClient> CreateSharingClient() {
    return std::make_unique<SharingClient>(
        webrtc_pc_factory_.get(), port_allocator_factory_.CreatePortAllocator(),
        base::BindOnce(
            &SharingWebRtcConnectionIntegrationTest::ConnectionClosed,
            base::Unretained(this)));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> webrtc_pc_factory_;

  FakePortAllocatorFactory port_allocator_factory_;
};

TEST_F(SharingWebRtcConnectionIntegrationTest, SendMessage_Success) {
  auto client_1 = CreateSharingClient();
  auto client_2 = CreateSharingClient();

  client_1->ConnectTo(client_2.get());
  client_2->ConnectTo(client_1.get());

  std::vector<uint8_t> data(1024, /*random_data=*/42);

  base::RunLoop receive_run_loop;
  EXPECT_CALL(client_2->host(), OnMessageReceived(testing::_))
      .WillOnce(testing::Invoke([&](const std::vector<uint8_t>& received) {
        EXPECT_EQ(data, received);
        receive_run_loop.Quit();
      }));

  base::RunLoop send_run_loop;
  client_1->connection().SendMessage(
      data, base::BindLambdaForTesting([&](mojom::SendMessageResult result) {
        EXPECT_EQ(mojom::SendMessageResult::kSuccess, result);
        send_run_loop.Quit();
      }));

  send_run_loop.Run();
  receive_run_loop.Run();
}

}  // namespace sharing
