// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/sharing_webrtc_connection.h"

#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/webrtc/test/mock_sharing_connection_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/test/mock_peerconnectioninterface.h"

namespace {

class MockPeerConnectionFactory
    : public rtc::RefCountedObject<webrtc::PeerConnectionFactoryInterface> {
 public:
  MOCK_METHOD1(SetOptions, void(const Options&));

  MOCK_METHOD1(
      CreateLocalMediaStream,
      rtc::scoped_refptr<webrtc::MediaStreamInterface>(const std::string&));

  MOCK_METHOD1(CreateAudioSource,
               rtc::scoped_refptr<webrtc::AudioSourceInterface>(
                   const cricket::AudioOptions&));

  MOCK_METHOD2(CreateVideoTrack,
               rtc::scoped_refptr<webrtc::VideoTrackInterface>(
                   const std::string&,
                   webrtc::VideoTrackSourceInterface*));

  MOCK_METHOD2(CreateAudioTrack,
               rtc::scoped_refptr<webrtc::AudioTrackInterface>(
                   const std::string&,
                   webrtc::AudioSourceInterface*));

  MOCK_METHOD0(StopAecDump, void());

  MOCK_METHOD2(CreatePeerConnection,
               rtc::scoped_refptr<webrtc::PeerConnectionInterface>(
                   const webrtc::PeerConnectionInterface::RTCConfiguration&,
                   webrtc::PeerConnectionDependencies));
};

class MockDataChannel
    : public rtc::RefCountedObject<webrtc::DataChannelInterface> {
 public:
  MOCK_METHOD1(RegisterObserver, void(webrtc::DataChannelObserver*));
  MOCK_METHOD0(UnregisterObserver, void());

  MOCK_CONST_METHOD0(label, std::string());

  MOCK_CONST_METHOD0(reliable, bool());
  MOCK_CONST_METHOD0(id, int());
  MOCK_CONST_METHOD0(state, DataState());
  MOCK_CONST_METHOD0(messages_sent, uint32_t());
  MOCK_CONST_METHOD0(bytes_sent, uint64_t());
  MOCK_CONST_METHOD0(messages_received, uint32_t());
  MOCK_CONST_METHOD0(bytes_received, uint64_t());

  MOCK_CONST_METHOD0(buffered_amount, uint64_t());

  MOCK_METHOD0(Close, void());

  MOCK_METHOD1(Send, bool(const webrtc::DataBuffer&));
};

}  // namespace

namespace sharing {

// TODO(knollr): Add tests for public interface and main data flows.
class SharingWebRtcConnectionTest : public testing::Test {
 public:
  SharingWebRtcConnectionTest() {
    mock_webrtc_pc_factory_ = new MockPeerConnectionFactory();
    mock_webrtc_pc_ = new webrtc::MockPeerConnectionInterface();
    mock_data_channel_ = new MockDataChannel();

    EXPECT_CALL(*mock_webrtc_pc_factory_,
                CreatePeerConnection(testing::_, testing::_))
        .WillOnce(testing::Return(mock_webrtc_pc_));
    EXPECT_CALL(*mock_webrtc_pc_, CreateDataChannel(testing::_, testing::_))
        .Times(testing::AtMost(1))
        .WillOnce(testing::Return(mock_data_channel_));
    EXPECT_CALL(*mock_data_channel_, state())
        .WillRepeatedly(
            testing::Return(webrtc::DataChannelInterface::DataState::kOpen));
    EXPECT_CALL(*mock_data_channel_, buffered_amount())
        .WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*mock_data_channel_, Send(testing::_))
        .WillRepeatedly(testing::Invoke([&](const webrtc::DataBuffer& data) {
          connection()->OnBufferedAmountChange(data.size());
          return true;
        }));

    connection_ = std::make_unique<SharingWebRtcConnection>(
        mock_webrtc_pc_factory_.get(), std::vector<mojom::IceServerPtr>(),
        connection_host_.signalling_sender.BindNewPipeAndPassRemote(),
        connection_host_.signalling_receiver.BindNewPipeAndPassReceiver(),
        connection_host_.delegate.BindNewPipeAndPassRemote(),
        connection_host_.connection.BindNewPipeAndPassReceiver(),
        connection_host_.socket_manager.BindNewPipeAndPassRemote(),
        connection_host_.mdns_responder.BindNewPipeAndPassRemote(),
        base::BindOnce(&SharingWebRtcConnectionTest::ConnectionClosed,
                       base::Unretained(this)));
    EXPECT_CALL(*mock_data_channel_, RegisterObserver(connection_.get()))
        .Times(testing::AtMost(1));
  }

  ~SharingWebRtcConnectionTest() override {
    EXPECT_CALL(*mock_webrtc_pc_, Close());
    connection_.reset();
    // Let libjingle threads finish.
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD1(ConnectionClosed, void(SharingWebRtcConnection*));

  SharingWebRtcConnection* connection() { return connection_.get(); }

  mojom::SendMessageResult SendMessageBlocking(
      const std::vector<uint8_t>& data) {
    mojom::SendMessageResult send_result = mojom::SendMessageResult::kError;
    base::RunLoop run_loop;
    connection()->SendMessage(
        data, base::BindLambdaForTesting([&](mojom::SendMessageResult result) {
          send_result = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    return send_result;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  rtc::scoped_refptr<MockPeerConnectionFactory> mock_webrtc_pc_factory_;
  rtc::scoped_refptr<webrtc::MockPeerConnectionInterface> mock_webrtc_pc_;
  rtc::scoped_refptr<MockDataChannel> mock_data_channel_;
  std::unique_ptr<SharingWebRtcConnection> connection_;
  MockSharingConnectionHost connection_host_;
};

TEST_F(SharingWebRtcConnectionTest, SendMessage_Empty) {
  std::vector<uint8_t> data;
  EXPECT_CALL(*this, ConnectionClosed(testing::_));
  EXPECT_EQ(mojom::SendMessageResult::kError, SendMessageBlocking(data));
}

TEST_F(SharingWebRtcConnectionTest, SendMessage_256kBLimit) {
  // Expect 256kB packets to succeed.
  std::vector<uint8_t> data(256 * 1024, 0);
  EXPECT_EQ(mojom::SendMessageResult::kSuccess, SendMessageBlocking(data));

  // Add one more byte and expect it to fail.
  data.push_back(0);
  EXPECT_CALL(*mock_data_channel_, Close());
  EXPECT_EQ(mojom::SendMessageResult::kError, SendMessageBlocking(data));
}

}  // namespace sharing
