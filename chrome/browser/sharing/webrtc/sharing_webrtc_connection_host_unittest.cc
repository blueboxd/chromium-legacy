// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/webrtc/sharing_webrtc_connection_host.h"

#include <algorithm>

#include "base/bind_helpers.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/fake_device_info.h"
#include "chrome/browser/sharing/fake_sharing_handler_registry.h"
#include "chrome/browser/sharing/mock_sharing_message_handler.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/webrtc/webrtc_signalling_host_fcm.h"
#include "chrome/services/sharing/public/mojom/webrtc.mojom.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/fake_gcm_driver.h"
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

// Implementation of GCMDriver that does not encrypt / decrypt messages.
class CustomFakeGCMDriver : public gcm::FakeGCMDriver {
 public:
  CustomFakeGCMDriver() = default;
  ~CustomFakeGCMDriver() override = default;

  void EncryptMessage(const std::string& app_id,
                      const std::string& authorized_entity,
                      const std::string& p256dh,
                      const std::string& auth_secret,
                      const std::string& message,
                      EncryptMessageCallback callback) override {
    std::string result = message;
    if (reverse_data_)
      std::reverse(result.begin(), result.end());

    std::move(callback).Run(fail_
                                ? gcm::GCMEncryptionResult::ENCRYPTION_FAILED
                                : gcm::GCMEncryptionResult::ENCRYPTED_DRAFT_08,
                            std::move(result));
  }

  void DecryptMessage(const std::string& app_id,
                      const std::string& authorized_entity,
                      const std::string& message,
                      DecryptMessageCallback callback) override {
    std::string result = message;
    if (reverse_data_)
      std::reverse(result.begin(), result.end());

    std::move(callback).Run(
        fail_ ? gcm::GCMDecryptionResult::INVALID_ENCRYPTION_HEADER
              : gcm::GCMDecryptionResult::DECRYPTED_DRAFT_08,
        std::move(result));
  }

  void set_reverse_data(bool reverse_data) { reverse_data_ = reverse_data; }

  void set_fail(bool fail) { fail_ = fail; }

 private:
  bool reverse_data_ = false;
  bool fail_ = false;
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

// Metric names
const char kWebRtcTimeout[] = "Sharing.WebRtc.Timeout";

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

    SharingWebRtcConnectionHost::EncryptionInfo encryption_info{
        "authorized_entity", "p256dh", "auth_secret"};

    host_ = std::make_unique<SharingWebRtcConnectionHost>(
        std::move(signalling_host), &handler_registry_, &fake_gcm_driver_,
        std::move(encryption_info),
        base::BindOnce(&SharingWebRtcConnectionHostTest::ConnectionClosed,
                       base::Unretained(this)),
        mock_service_.delegate.BindNewPipeAndPassReceiver(),
        mock_service_.connection.BindNewPipeAndPassRemote(),
        mock_service_.socket_manager_client.BindNewPipeAndPassReceiver(),
        mock_service_.socket_manager.BindNewPipeAndPassRemote());
  }

  MOCK_METHOD0(ConnectionClosed, void());

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

  void WaitForConnectionClosed() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, ConnectionClosed()).WillOnce(testing::Invoke([&]() {
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  void WaitForMojoDisconnect() {
    base::RunLoop run_loop;
    mock_service_.delegate.set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(mock_service_.delegate.is_connected());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockSharingMessageHandler message_handler_;
  MockSharingMessageHandler ack_message_handler_;
  MockSharingMojoService mock_service_;
  FakeSharingHandlerRegistry handler_registry_;
  CustomFakeGCMDriver fake_gcm_driver_;
  MockSignallingHost* signalling_host_;
  std::unique_ptr<SharingWebRtcConnectionHost> host_;
  base::HistogramTester histograms_;
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

TEST_F(SharingWebRtcConnectionHostTest, DecryptMessage_Success) {
  fake_gcm_driver_.set_reverse_data(true);

  ExpectOnMessage(&message_handler_);
  ExpectSendMessage();

  base::RunLoop run_loop;
  mock_service_.delegate.set_disconnect_handler(run_loop.QuitClosure());

  auto message = SerializeMessage(CreateMessage());
  // Reverse data to simulate encrypted message being received.
  std::reverse(message.begin(), message.end());
  host_->OnMessageReceived(message);

  run_loop.Run();
}

TEST_F(SharingWebRtcConnectionHostTest, DecryptMessage_Fail) {
  fake_gcm_driver_.set_fail(true);

  // Expect not to handle the message as decryption failed.
  EXPECT_CALL(message_handler_, OnMessage(testing::_, testing::_)).Times(0);

  auto message = SerializeMessage(CreateMessage());
  // Reverse data to simulate encrypted message being received.
  std::reverse(message.begin(), message.end());
  host_->OnMessageReceived(message);

  WaitForConnectionClosed();
}

TEST_F(SharingWebRtcConnectionHostTest, DecryptMessage_FailNotEncrypted) {
  fake_gcm_driver_.set_reverse_data(true);

  // Expect not to handle the message as decryption failed.
  EXPECT_CALL(message_handler_, OnMessage(testing::_, testing::_)).Times(0);

  auto message = SerializeMessage(CreateMessage());
  // Do not reverse data to check if we actually try to decrypt.
  host_->OnMessageReceived(message);

  WaitForConnectionClosed();
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

TEST_F(SharingWebRtcConnectionHostTest, EncryptMessage_Success) {
  fake_gcm_driver_.set_reverse_data(true);

  auto message = CreateMessage();
  auto serialized_message = SerializeMessage(message);
  std::reverse(serialized_message.begin(), serialized_message.end());

  EXPECT_CALL(mock_service_, SendMessage(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const std::vector<uint8_t>& data,
              sharing::mojom::SharingWebRtcConnection::SendMessageCallback
                  callback) {
            EXPECT_EQ(serialized_message, data);
            std::move(callback).Run(
                sharing::mojom::SendMessageResult::kSuccess);
          }));

  base::RunLoop run_loop;
  host_->SendMessage(
      std::move(message),
      base::BindLambdaForTesting([&](sharing::mojom::SendMessageResult result) {
        EXPECT_EQ(sharing::mojom::SendMessageResult::kSuccess, result);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SharingWebRtcConnectionHostTest, EncryptMessage_Fail) {
  fake_gcm_driver_.set_fail(true);

  EXPECT_CALL(mock_service_, SendMessage(testing::_, testing::_)).Times(0);

  host_->SendMessage(
      CreateMessage(),
      base::BindLambdaForTesting([&](sharing::mojom::SendMessageResult result) {
        EXPECT_EQ(sharing::mojom::SendMessageResult::kError, result);
      }));

  WaitForConnectionClosed();
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
  EXPECT_CALL(*this, ConnectionClosed()).WillOnce(testing::Invoke([&]() {
    run_loop.Quit();
  }));

  // Expect the connection to force close if the network service connection is
  // lost. This also happens if the Sharing service closes the connection.
  mock_service_.socket_manager.reset();
  run_loop.Run();
}

TEST_F(SharingWebRtcConnectionHostTest, ConnectionTimeout) {
  base::TimeTicks start_time = task_environment_.NowTicks();

  // Simply wait for the connection to be force closed.
  WaitForConnectionClosed();

  base::TimeTicks end_time = task_environment_.NowTicks();
  EXPECT_EQ(start_time + kSharingWebRtcTimeout, end_time);
  histograms_.ExpectUniqueSample(kWebRtcTimeout,
                                 sharing::WebRtcTimeoutState::kConnecting, 1);
}

TEST_F(SharingWebRtcConnectionHostTest, ConnectionTimeout_OnMessageReceived) {
  // Wait for some time before receiving the first message.
  task_environment_.FastForwardBy(kSharingWebRtcTimeout / 2);

  base::TimeTicks start_time = task_environment_.NowTicks();

  // Expect the message handler to be called but don't call the callback.
  EXPECT_CALL(message_handler_, OnMessage(testing::_, testing::_));
  host_->OnMessageReceived(SerializeMessage(CreateMessage()));

  // The call to OnMessageReceived should have reset the timer.
  WaitForConnectionClosed();
  base::TimeTicks end_time = task_environment_.NowTicks();
  EXPECT_EQ(start_time + kSharingWebRtcTimeout, end_time);
  histograms_.ExpectUniqueSample(
      kWebRtcTimeout, sharing::WebRtcTimeoutState::kMessageReceived, 1);
}

TEST_F(SharingWebRtcConnectionHostTest, ConnectionTimeout_OnConnectionClosing) {
  // Wait for some time before receiving the first message.
  task_environment_.FastForwardBy(kSharingWebRtcTimeout / 2);

  // Receive an Ack message that will start closing the connection.
  ExpectOnMessage(&ack_message_handler_);
  host_->OnMessageReceived(SerializeMessage(CreateAckMessage()));

  // The timer should reset when closing the connection.
  base::TimeTicks start_time = task_environment_.NowTicks();
  WaitForConnectionClosed();
  base::TimeTicks end_time = task_environment_.NowTicks();
  histograms_.ExpectUniqueSample(
      kWebRtcTimeout, sharing::WebRtcTimeoutState::kDisconnecting, 1);

  EXPECT_EQ(start_time + kSharingWebRtcTimeout, end_time);
}

TEST_F(SharingWebRtcConnectionHostTest, ConnectionTimeout_SendMessage) {
  // Wait for some time before sending the first message.
  task_environment_.FastForwardBy(kSharingWebRtcTimeout / 2);

  base::TimeTicks start_time = task_environment_.NowTicks();

  // Hold on to the SendMessageCallback without calling it.
  sharing::mojom::SharingWebRtcConnection::SendMessageCallback send_callback;
  EXPECT_CALL(mock_service_, SendMessage(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const std::vector<uint8_t>& data,
              sharing::mojom::SharingWebRtcConnection::SendMessageCallback
                  callback) { send_callback = std::move(callback); }));

  // The NullCallback will not be called.
  host_->SendMessage(CreateMessage(), base::NullCallback());

  // The call to SendMessage should have reset the timer.
  WaitForConnectionClosed();
  base::TimeTicks end_time = task_environment_.NowTicks();
  EXPECT_EQ(start_time + kSharingWebRtcTimeout, end_time);
  histograms_.ExpectUniqueSample(kWebRtcTimeout,
                                 sharing::WebRtcTimeoutState::kMessageSent, 1);

  WaitForMojoDisconnect();
}
