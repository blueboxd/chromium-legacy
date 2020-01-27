// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/webrtc/sharing_webrtc_connection_host.h"

#include "base/bind_helpers.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/sharing/fake_device_info.h"
#include "chrome/browser/sharing/fake_sharing_handler_registry.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/webrtc/webrtc_signalling_host_fcm.h"
#include "chrome/services/sharing/public/mojom/webrtc.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSharingMojoService : public sharing::mojom::SharingWebRtcConnection,
                               public network::mojom::P2PTrustedSocketManager {
 public:
  MockSharingMojoService() = default;
  ~MockSharingMojoService() override = default;

  // sharing::mojom::SharingWebRtcConnection:
  MOCK_METHOD2(SendMessage,
               void(const std::vector<uint8_t>&, SendMessageCallback));

  // network::mojom::P2PTrustedSocketManager:
  void StartRtpDump(bool incoming, bool outgoing) override {}
  void StopRtpDump(bool incoming, bool outgoing) override {}

  mojo::Remote<sharing::mojom::SharingWebRtcConnectionDelegate> delegate;
  mojo::Receiver<sharing::mojom::SharingWebRtcConnection> connection{this};
  mojo::Remote<network::mojom::P2PTrustedSocketManagerClient>
      socket_manager_client;
  mojo::Receiver<network::mojom::P2PTrustedSocketManager> socket_manager{this};
};

class MockSignallingHost : public WebRtcSignallingHostFCM {
 public:
  MockSignallingHost()
      : WebRtcSignallingHostFCM(
            mojo::PendingReceiver<sharing::mojom::SignallingSender>(),
            mojo::PendingRemote<sharing::mojom::SignallingReceiver>(),
            /*message_sender=*/nullptr,
            CreateFakeDeviceInfo("id", "name")) {}
  ~MockSignallingHost() override = default;

  // WebRtcSignallingHostFCM:
  MOCK_METHOD2(SendOffer, void(const std::string&, SendOfferCallback));
  MOCK_METHOD1(SendIceCandidates,
               void(std::vector<sharing::mojom::IceCandidatePtr>));
  MOCK_METHOD2(OnOfferReceived, void(const std::string&, SendOfferCallback));
  MOCK_METHOD1(OnIceCandidatesReceived,
               void(std::vector<sharing::mojom::IceCandidatePtr>));
};

class MockSharingMessageHandler : public SharingMessageHandler {
 public:
  MockSharingMessageHandler() = default;
  ~MockSharingMessageHandler() override = default;

  // SharingMessageHandler:
  MOCK_METHOD2(OnMessage,
               void(chrome_browser_sharing::SharingMessage, DoneCallback));
};

chrome_browser_sharing::WebRtcMessage CreateMessage() {
  chrome_browser_sharing::WebRtcMessage message;
  message.set_message_guid("guid");
  chrome_browser_sharing::SharingMessage* sharing_message =
      message.mutable_message();
  chrome_browser_sharing::SharedClipboardMessage* shared_clipboard_message =
      sharing_message->mutable_shared_clipboard_message();
  shared_clipboard_message->set_text("text");
  return message;
}

chrome_browser_sharing::WebRtcMessage CreateAckMessage() {
  chrome_browser_sharing::WebRtcMessage message;
  chrome_browser_sharing::SharingMessage* sharing_message =
      message.mutable_message();
  chrome_browser_sharing::AckMessage* ack_message =
      sharing_message->mutable_ack_message();
  ack_message->set_original_message_id("original_message_id");
  return message;
}

std::vector<uint8_t> SerializeMessage(
    const chrome_browser_sharing::WebRtcMessage& message) {
  std::vector<uint8_t> serialized_message(message.ByteSize());
  message.SerializeToArray(serialized_message.data(),
                           serialized_message.size());
  return serialized_message;
}

}  // namespace

class SharingWebRtcConnectionHostTest : public testing::Test {
 public:
  SharingWebRtcConnectionHostTest() {
    handler_registry_.SetSharingHandler(
        chrome_browser_sharing::SharingMessage::kSharedClipboardMessage,
        &message_handler_);
    handler_registry_.SetSharingHandler(
        chrome_browser_sharing::SharingMessage::kAckMessage,
        &ack_message_handler_);

    auto signalling_host = std::make_unique<MockSignallingHost>();
    signalling_host_ = signalling_host.get();

    host_ = std::make_unique<SharingWebRtcConnectionHost>(
        std::move(signalling_host), &handler_registry_,
        CreateFakeDeviceInfo("id", "name"),
        base::BindOnce(&SharingWebRtcConnectionHostTest::ConnectionClosed,
                       base::Unretained(this)),
        mock_service_.delegate.BindNewPipeAndPassReceiver(),
        mock_service_.connection.BindNewPipeAndPassRemote(),
        mock_service_.socket_manager_client.BindNewPipeAndPassReceiver(),
        mock_service_.socket_manager.BindNewPipeAndPassRemote());
  }

  MOCK_METHOD1(ConnectionClosed, void(const std::string&));

  void ExpectOnMessage(MockSharingMessageHandler* handler) {
    EXPECT_CALL(*handler, OnMessage(testing::_, testing::_))
        .WillOnce(testing::Invoke(
            [&](const chrome_browser_sharing::SharingMessage& message,
                SharingMessageHandler::DoneCallback done_callback) {
              std::move(done_callback).Run(/*response=*/nullptr);
            }));
  }

  void ExpectSendMessage() {
    EXPECT_CALL(mock_service_, SendMessage(testing::_, testing::_))
        .WillOnce(testing::Invoke(
            [&](const std::vector<uint8_t>& data,
                sharing::mojom::SharingWebRtcConnection::SendMessageCallback
                    callback) {
              std::move(callback).Run(
                  sharing::mojom::SendMessageResult::kSuccess);
            }));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  MockSharingMessageHandler message_handler_;
  MockSharingMessageHandler ack_message_handler_;
  MockSharingMojoService mock_service_;
  FakeSharingHandlerRegistry handler_registry_;
  MockSignallingHost* signalling_host_;
  std::unique_ptr<SharingWebRtcConnectionHost> host_;
};

TEST_F(SharingWebRtcConnectionHostTest, OnMessageReceived) {
  EXPECT_TRUE(mock_service_.delegate.is_connected());

  // Expect the message handler to be called.
  ExpectOnMessage(&message_handler_);
  // Expect that an Ack message is sent after the message handler is done.
  ExpectSendMessage();

  // Expect that sending the Ack message closes the connection.
  base::RunLoop run_loop;
  mock_service_.delegate.set_disconnect_handler(run_loop.QuitClosure());

  host_->OnMessageReceived(SerializeMessage(CreateMessage()));
  run_loop.Run();

  EXPECT_FALSE(mock_service_.delegate.is_connected());
}

TEST_F(SharingWebRtcConnectionHostTest, OnAckMessageReceived) {
  EXPECT_TRUE(mock_service_.delegate.is_connected());

  // Expect the Ack message handler to be called.
  ExpectOnMessage(&ack_message_handler_);

  // Expect that handling the Ack message closes the connection.
  base::RunLoop run_loop;
  mock_service_.delegate.set_disconnect_handler(run_loop.QuitClosure());

  host_->OnMessageReceived(SerializeMessage(CreateAckMessage()));
  run_loop.Run();

  EXPECT_FALSE(mock_service_.delegate.is_connected());
}

TEST_F(SharingWebRtcConnectionHostTest, SendMessage) {
  // Expect the message to be sent to the service.
  ExpectSendMessage();

  base::RunLoop run_loop;
  host_->SendMessage(
      CreateMessage(),
      base::BindLambdaForTesting([&](sharing::mojom::SendMessageResult result) {
        EXPECT_EQ(sharing::mojom::SendMessageResult::kSuccess, result);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SharingWebRtcConnectionHostTest, OnOfferReceived) {
  EXPECT_CALL(*signalling_host_, OnOfferReceived("offer", testing::_))
      .WillOnce(testing::Invoke(
          [&](const std::string& offer,
              base::OnceCallback<void(const std::string&)> callback) {
            EXPECT_EQ("offer", offer);
            std::move(callback).Run("answer");
          }));

  base::RunLoop run_loop;
  host_->OnOfferReceived(
      "offer", base::BindLambdaForTesting([&](const std::string& answer) {
        EXPECT_EQ("answer", answer);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SharingWebRtcConnectionHostTest, OnIceCandidatesReceived) {
  base::RunLoop run_loop;
  EXPECT_CALL(*signalling_host_, OnIceCandidatesReceived(testing::_))
      .WillOnce(testing::Invoke(
          [&](std::vector<sharing::mojom::IceCandidatePtr> ice_candidates) {
            EXPECT_EQ(1u, ice_candidates.size());
            run_loop.Quit();
          }));

  std::vector<sharing::mojom::IceCandidatePtr> ice_candidates;
  ice_candidates.push_back(sharing::mojom::IceCandidate::New());
  host_->OnIceCandidatesReceived(std::move(ice_candidates));
  run_loop.Run();
}

TEST_F(SharingWebRtcConnectionHostTest, ConnectionClosed) {
  base::RunLoop run_loop;
  EXPECT_CALL(*this, ConnectionClosed(testing::_))
      .WillOnce(testing::Invoke(
          [&](const std::string& device_guid) { run_loop.Quit(); }));

  // Expect the connection to force close if the network service connection is
  // lost. This also happens if the Sharing service closes the connection.
  mock_service_.socket_manager.reset();
  run_loop.Run();
}
