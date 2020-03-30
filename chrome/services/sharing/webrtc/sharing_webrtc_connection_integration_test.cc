// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/sharing_webrtc_connection.h"

#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/webrtc/test/mock_sharing_connection_host.h"
#include "jingle/glue/thread_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/test/mock_peerconnectioninterface.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace {

class MockP2PSocket : public network::mojom::P2PSocket {
 public:
  MOCK_METHOD3(Send,
               void(const std::vector<int8_t>&,
                    const network::P2PPacketInfo&,
                    const net::MutableNetworkTrafficAnnotationTag&));

  MOCK_METHOD2(SetOption, void(network::P2PSocketOption, int32_t));
};

class SharingClient {
 public:
  SharingClient(
      webrtc::PeerConnectionFactoryInterface* pc_factory,
      base::OnceCallback<void(sharing::SharingWebRtcConnection*)> on_disconnect)
      : connection_(pc_factory,
                    /*ice_servers=*/{},
                    host_.signalling_sender.BindNewPipeAndPassRemote(),
                    host_.signalling_receiver.BindNewPipeAndPassReceiver(),
                    host_.delegate.BindNewPipeAndPassRemote(),
                    host_.connection.BindNewPipeAndPassReceiver(),
                    host_.socket_manager.BindNewPipeAndPassRemote(),
                    host_.mdns_responder.BindNewPipeAndPassRemote(),
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

    EXPECT_CALL(mock_socket_, Send(testing::_, testing::_, testing::_))
        .Times(testing::AtLeast(0))
        .WillRepeatedly(testing::Invoke(
            [client](const std::vector<int8_t>& data,
                     const network::P2PPacketInfo& info,
                     const net::MutableNetworkTrafficAnnotationTag& tag) {
              client->socket_client_->DataReceived(info.destination, data,
                                                   base::TimeTicks());
            }));
  }

  void FakeNetwork(const net::IPAddress& address, const net::IPAddress& other) {
    EXPECT_CALL(host_, StartNetworkNotifications(testing::_))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<
                network::mojom::P2PNetworkNotificationClient> client) {
              net::NetworkInterfaceList list;
              list.push_back(net::NetworkInterface(
                  "ifname", "ifname", /*index=*/0,
                  net::NetworkChangeNotifier::CONNECTION_ETHERNET, address,
                  /*prefixlen=*/24, net::IP_ADDRESS_ATTRIBUTE_NONE));

              network_notification_client_.Bind(std::move(client));
              network_notification_client_->NetworkListChanged(
                  list, address, net::IPAddress());
            }));

    EXPECT_CALL(host_, CreateNameForAddress(testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(
            [&](const net::IPAddress& address,
                network::mojom::MdnsResponder::CreateNameForAddressCallback
                    callback) { std::move(callback).Run("name", false); }));

    EXPECT_CALL(host_, GetHostAddress(testing::_, testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(
            [other](const std::string& name, bool mdns,
                    network::mojom::P2PSocketManager::GetHostAddressCallback
                        callback) {
              net::IPAddressList list;
              list.push_back(other);
              std::move(callback).Run(list);
            }));

    EXPECT_CALL(host_, CreateSocket(testing::_, testing::_, testing::_,
                                    testing::_, testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(
            [&](network::P2PSocketType type,
                const net::IPEndPoint& local_address,
                const network::P2PPortRange& range,
                const network::P2PHostAndIPEndPoint& remote_address,
                mojo::PendingRemote<network::mojom::P2PSocketClient>
                    socket_client,
                mojo::PendingReceiver<network::mojom::P2PSocket> socket) {
              socket_client_.Bind(std::move(socket_client));
              socket_.Bind(std::move(socket));

              socket_client_->SocketCreated(
                  net::IPEndPoint(local_address.address(), /*port=*/8080),
                  remote_address.ip_address);
            }));
  }

 private:
  sharing::MockSharingConnectionHost host_;
  sharing::SharingWebRtcConnection connection_;
  mojo::Remote<network::mojom::P2PNetworkNotificationClient>
      network_notification_client_;
  MockP2PSocket mock_socket_;
  mojo::Receiver<network::mojom::P2PSocket> socket_{&mock_socket_};
  mojo::Remote<network::mojom::P2PSocketClient> socket_client_;
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
        webrtc_pc_factory_.get(),
        base::BindOnce(
            &SharingWebRtcConnectionIntegrationTest::ConnectionClosed,
            base::Unretained(this)));
  }

  enum class Role { kSender, kReceiver, kSenderAndReceiver };

  void ExpectTimingHistogram(const std::string& name, Role role) {
    int expected_count = role == Role::kSenderAndReceiver ? 2 : 1;
    histograms_.ExpectTotalCount(
        base::StrCat({"Sharing.WebRtc.TimingEvents.", name}), expected_count);

    if (role == Role::kSender || role == Role::kSenderAndReceiver) {
      histograms_.ExpectTotalCount(
          base::StrCat({"Sharing.WebRtc.TimingEvents.Sender.", name}), 1);
    }

    if (role == Role::kReceiver || role == Role::kSenderAndReceiver) {
      histograms_.ExpectTotalCount(
          base::StrCat({"Sharing.WebRtc.TimingEvents.Receiver.", name}), 1);
    }
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> webrtc_pc_factory_;
  base::HistogramTester histograms_;
};

TEST_F(SharingWebRtcConnectionIntegrationTest, SendMessage_Success) {
  auto client_1 = CreateSharingClient();
  auto client_2 = CreateSharingClient();

  client_1->ConnectTo(client_2.get());
  client_2->ConnectTo(client_1.get());

  // Mock responses from the network service.
  net::IPAddress address_1(192, 168, 0, 1);
  net::IPAddress address_2(192, 168, 0, 2);
  client_1->FakeNetwork(address_1, address_2);
  client_2->FakeNetwork(address_2, address_1);

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

  ExpectTimingHistogram("OfferCreated", Role::kSender);
  ExpectTimingHistogram("AnswerReceived", Role::kSender);
  ExpectTimingHistogram("QueuingMessage", Role::kSender);
  ExpectTimingHistogram("SendingMessage", Role::kSender);
  ExpectTimingHistogram("OfferReceived", Role::kReceiver);
  ExpectTimingHistogram("AnswerCreated", Role::kReceiver);
  ExpectTimingHistogram("MessageReceived", Role::kReceiver);
  ExpectTimingHistogram("SignalingStable", Role::kSenderAndReceiver);
  ExpectTimingHistogram("IceCandidateReceived", Role::kSenderAndReceiver);
  ExpectTimingHistogram("DataChannelOpen", Role::kSenderAndReceiver);
}

}  // namespace sharing
