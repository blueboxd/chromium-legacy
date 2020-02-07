// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/sharing_webrtc_connection.h"

#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/webrtc/test/mock_sharing_connection_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/candidate.h"
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

class MockPeerConnection : public webrtc::MockPeerConnectionInterface {
 public:
  using webrtc::MockPeerConnectionInterface::AddIceCandidate;
  MOCK_METHOD2(AddIceCandidate,
               void(std::unique_ptr<webrtc::IceCandidateInterface> candidate,
                    std::function<void(webrtc::RTCError)> callback));
};

class MockSessionDescriptionInterface
    : public webrtc::SessionDescriptionInterface {
 public:
  MOCK_METHOD0(description, cricket::SessionDescription*());
  MOCK_CONST_METHOD0(description, cricket::SessionDescription*());

  MOCK_CONST_METHOD0(session_id, std::string());
  MOCK_CONST_METHOD0(session_version, std::string());

  MOCK_CONST_METHOD0(GetType, webrtc::SdpType());

  MOCK_CONST_METHOD0(type, std::string());

  MOCK_METHOD1(AddCandidate, bool(const webrtc::IceCandidateInterface*));

  MOCK_METHOD1(RemoveCandidates,
               size_t(const std::vector<cricket::Candidate>& candidates));

  MOCK_CONST_METHOD0(number_of_mediasections, size_t());

  MOCK_CONST_METHOD1(
      candidates,
      webrtc::IceCandidateCollection*(size_t mediasection_index));

  MOCK_CONST_METHOD1(ToString, bool(std::string* out));
};

std::unique_ptr<webrtc::IceCandidateInterface> CreateIceCandidate(int index) {
  const int base_port = 1234;
  rtc::SocketAddress address("127.0.0.1", base_port + index);
  cricket::Candidate candidate(cricket::ICE_CANDIDATE_COMPONENT_RTP, "udp",
                               address, 1, "", "", "local", 0, "1");
  return webrtc::CreateIceCandidate("mid", index, candidate);
}

std::vector<sharing::mojom::IceCandidatePtr> GenerateIceCandidates(
    int invalid_candidates,
    int valid_candidates) {
  std::vector<sharing::mojom::IceCandidatePtr> ice_candidates;

  // Add invalid ice candidate first;
  for (int i = 0; i < invalid_candidates; i++) {
    sharing::mojom::IceCandidatePtr ice_candidate_ptr(
        sharing::mojom::IceCandidate::New());
    ice_candidate_ptr->candidate = "invalid_candidate";
    ice_candidate_ptr->sdp_mid = "random_sdp_mid";
    ice_candidate_ptr->sdp_mline_index = i;
    ice_candidates.push_back(std::move(ice_candidate_ptr));
  }

  // Add valid ice candidates;
  for (int i = 0; i < valid_candidates; i++) {
    auto ice_candidate = CreateIceCandidate(i);
    sharing::mojom::IceCandidatePtr ice_candidate_ptr(
        sharing::mojom::IceCandidate::New());
    ice_candidate->ToString(&ice_candidate_ptr->candidate);
    ice_candidate_ptr->sdp_mid = ice_candidate->sdp_mid();
    ice_candidate_ptr->sdp_mline_index = ice_candidate->sdp_mline_index();
    ice_candidates.push_back(std::move(ice_candidate_ptr));
  }

  return ice_candidates;
}

}  // namespace

namespace sharing {

// TODO(knollr): Add tests for public interface and main data flows.
class SharingWebRtcConnectionTest : public testing::Test {
 public:
  SharingWebRtcConnectionTest() {
    mock_webrtc_pc_factory_ = new MockPeerConnectionFactory();
    mock_webrtc_pc_ = new MockPeerConnection();
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
  rtc::scoped_refptr<MockPeerConnection> mock_webrtc_pc_;
  rtc::scoped_refptr<MockDataChannel> mock_data_channel_;
  std::unique_ptr<SharingWebRtcConnection> connection_;
  MockSharingConnectionHost connection_host_;
  base::HistogramTester histograms_;
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

TEST_F(SharingWebRtcConnectionTest, AddIceCandidates) {
  EXPECT_CALL(*this, ConnectionClosed(testing::_));

  const int valid_ice_candidates_count = 2;
  const int invalid_ice_candidates_count = 3;
  const std::string histogram_name = "Sharing.WebRtc.AddIceCandidate";

  MockSessionDescriptionInterface local_description;
  EXPECT_CALL(*mock_webrtc_pc_, local_description())
      .WillOnce(testing::Return(&local_description));
  EXPECT_CALL(*mock_webrtc_pc_, signaling_state())
      .WillOnce(testing::Return(
          webrtc::PeerConnectionInterface::SignalingState::kStable));
  EXPECT_CALL(*mock_webrtc_pc_, AddIceCandidate)
      .Times(valid_ice_candidates_count)
      .WillRepeatedly(testing::InvokeArgument<1>(webrtc::RTCError()));

  connection_->OnIceCandidatesReceived(GenerateIceCandidates(
      invalid_ice_candidates_count, valid_ice_candidates_count));

  histograms_.ExpectTotalCount(
      histogram_name,
      valid_ice_candidates_count + invalid_ice_candidates_count);
  histograms_.ExpectBucketCount(histogram_name, 0,
                                invalid_ice_candidates_count);
  histograms_.ExpectBucketCount(histogram_name, 1, valid_ice_candidates_count);
}

TEST_F(SharingWebRtcConnectionTest, AddIceCandidates_RTCError) {
  EXPECT_CALL(*this, ConnectionClosed(testing::_));

  const std::string histogram_name = "Sharing.WebRtc.AddIceCandidate";
  const int invalid_ice_candidates_count = 3;
  const int valid_ice_candidates_count = 2;

  // Count of ice candidates that will fail with internal error.
  const int valid_ice_candidates_internal_error = 1;

  MockSessionDescriptionInterface local_description;
  EXPECT_CALL(*mock_webrtc_pc_, local_description())
      .WillOnce(testing::Return(&local_description));
  EXPECT_CALL(*mock_webrtc_pc_, signaling_state())
      .WillOnce(testing::Return(
          webrtc::PeerConnectionInterface::SignalingState::kStable));
  EXPECT_CALL(*mock_webrtc_pc_, AddIceCandidate)
      .WillOnce(testing::InvokeArgument<1>(webrtc::RTCError()))
      .WillRepeatedly(testing::InvokeArgument<1>(
          webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR)));

  connection_->OnIceCandidatesReceived(GenerateIceCandidates(
      invalid_ice_candidates_count, valid_ice_candidates_count));

  histograms_.ExpectTotalCount(
      histogram_name,
      valid_ice_candidates_count + invalid_ice_candidates_count);
  histograms_.ExpectBucketCount(
      histogram_name, 0,
      invalid_ice_candidates_count + valid_ice_candidates_internal_error);
  histograms_.ExpectBucketCount(
      histogram_name, 1,
      valid_ice_candidates_count - valid_ice_candidates_internal_error);
}

TEST_F(SharingWebRtcConnectionTest, AddIceCandidates_BeforeSignaling) {
  // Local description not ready
  EXPECT_CALL(*mock_webrtc_pc_, local_description())
      .WillOnce(testing::Return(nullptr));

  EXPECT_CALL(*mock_webrtc_pc_, AddIceCandidate).Times(0);
  connection_->OnIceCandidatesReceived(GenerateIceCandidates(
      /*invalid_count=*/0, /*valid_count=*/1));

  // Signaling state not ready
  MockSessionDescriptionInterface local_description;
  EXPECT_CALL(*mock_webrtc_pc_, local_description())
      .WillOnce(testing::Return(&local_description));
  EXPECT_CALL(*mock_webrtc_pc_, signaling_state())
      .WillOnce(testing::Return(
          webrtc::PeerConnectionInterface::SignalingState::kHaveRemoteOffer));

  EXPECT_CALL(*mock_webrtc_pc_, AddIceCandidate).Times(0);
  connection_->OnIceCandidatesReceived(GenerateIceCandidates(
      /*invalid_count=*/0, /*valid_count=*/1));

  // Local description and signaling state ready
  EXPECT_CALL(*mock_webrtc_pc_, local_description())
      .WillOnce(testing::Return(&local_description));
  EXPECT_CALL(*mock_webrtc_pc_, signaling_state())
      .WillOnce(testing::Return(
          webrtc::PeerConnectionInterface::SignalingState::kStable));

  EXPECT_CALL(*mock_webrtc_pc_, AddIceCandidate).Times(3);
  connection_->OnIceCandidatesReceived(GenerateIceCandidates(
      /*invalid_count=*/0, /*valid_count=*/1));
}

TEST_F(SharingWebRtcConnectionTest, OnIceCandidate_BeforeSignaling) {
  auto ice_candidate = CreateIceCandidate(/*index=*/0);

  // Local description not ready
  EXPECT_CALL(*mock_webrtc_pc_, local_description())
      .WillOnce(testing::Return(nullptr));

  EXPECT_CALL(connection_host_, SendIceCandidates(testing::_)).Times(0);
  connection_->OnIceCandidate(ice_candidate.get());

  // Signaling state not ready
  MockSessionDescriptionInterface local_description;
  EXPECT_CALL(*mock_webrtc_pc_, local_description())
      .WillOnce(testing::Return(&local_description));
  EXPECT_CALL(*mock_webrtc_pc_, signaling_state())
      .WillOnce(testing::Return(
          webrtc::PeerConnectionInterface::SignalingState::kHaveRemoteOffer));

  EXPECT_CALL(connection_host_, SendIceCandidates(testing::_)).Times(0);
  connection_->OnIceCandidate(ice_candidate.get());

  // Local description and signaling state ready
  EXPECT_CALL(*mock_webrtc_pc_, local_description())
      .WillOnce(testing::Return(&local_description));
  EXPECT_CALL(*mock_webrtc_pc_, signaling_state())
      .WillOnce(testing::Return(
          webrtc::PeerConnectionInterface::SignalingState::kStable));

  EXPECT_CALL(connection_host_, SendIceCandidates(testing::_))
      .WillOnce(testing::Invoke(
          [](std::vector<mojom::IceCandidatePtr> ice_candidates) {
            EXPECT_EQ(3u, ice_candidates.size());
          }));
  connection_->OnIceCandidate(ice_candidate.get());
}

}  // namespace sharing
