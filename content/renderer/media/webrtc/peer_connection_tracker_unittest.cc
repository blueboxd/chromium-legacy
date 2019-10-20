// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/peer_connection_tracker.h"

#include "base/test/task_environment.h"
#include "content/common/media/peer_connection_tracker.mojom.h"
#include "content/renderer/media/webrtc/rtc_peer_connection_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_rtc_offer_options.h"
#include "third_party/blink/public/platform/web_rtc_rtp_receiver.h"
#include "third_party/blink/public/platform/web_rtc_rtp_sender.h"
#include "third_party/blink/public/platform/web_rtc_rtp_transceiver.h"
#include "third_party/blink/public/web/modules/peerconnection/fake_rtc_rtp_transceiver_impl.h"
#include "third_party/blink/public/web/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/public/web/modules/peerconnection/mock_web_rtc_peer_connection_handler_client.h"

using ::testing::_;

namespace content {

const char* kDefaultTransceiverString =
    "getTransceivers()[0]:{\n"
    "  mid:null,\n"
    "  sender:{\n"
    "    track:'senderTrackId',\n"
    "    streams:['senderStreamId'],\n"
    "  },\n"
    "  receiver:{\n"
    "    track:'receiverTrackId',\n"
    "    streams:['receiverStreamId'],\n"
    "  },\n"
    "  stopped:false,\n"
    "  direction:'sendonly',\n"
    "  currentDirection:null,\n"
    "}";

const char* kDefaultSenderString =
    "getSenders()[0]:{\n"
    "  track:'senderTrackId',\n"
    "  streams:['senderStreamId'],\n"
    "}";

const char* kDefaultReceiverString =
    "getReceivers()[0]:{\n"
    "  track:'receiverTrackId',\n"
    "  streams:['receiverStreamId'],\n"
    "}";

class MockPeerConnectionTrackerHost : public mojom::PeerConnectionTrackerHost {
 public:
  MockPeerConnectionTrackerHost() {}
  MOCK_METHOD3(UpdatePeerConnection,
               void(int, const std::string&, const std::string&));
  MOCK_METHOD1(AddPeerConnection, void(mojom::PeerConnectionInfoPtr));
  MOCK_METHOD1(RemovePeerConnection, void(int));
  MOCK_METHOD2(OnPeerConnectionSessionIdSet, void(int, const std::string&));
  MOCK_METHOD5(GetUserMedia,
               void(const std::string&,
                    bool,
                    bool,
                    const std::string&,
                    const std::string&));
  MOCK_METHOD2(WebRtcEventLogWrite, void(int, const std::string&));
  MOCK_METHOD2(AddStandardStats, void(int, base::Value));
  MOCK_METHOD2(AddLegacyStats, void(int, base::Value));

  mojo::Remote<mojom::PeerConnectionTrackerHost> CreatePendingRemoteAndBind() {
    receiver_.reset();
    return mojo::Remote<mojom::PeerConnectionTrackerHost>(
        receiver_.BindNewPipeAndPassRemote(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  }

  mojo::Receiver<mojom::PeerConnectionTrackerHost> receiver_{this};
};

// Creates a transceiver that is expected to be logged as
// |kDefaultTransceiverString|, |kDefaultSenderString| or
// |kDefaultReceiverString| depending on if |implementation_type| refers to a
// fully implemented, sender-only or receiver-only transceiver.
//
// This is used in unittests that don't care about the specific attributes of
// the transceiver.
std::unique_ptr<blink::WebRTCRtpTransceiver> CreateDefaultTransceiver(
    blink::WebRTCRtpTransceiverImplementationType implementation_type) {
  std::unique_ptr<blink::WebRTCRtpTransceiver> transceiver;
  blink::FakeRTCRtpSenderImpl sender(
      "senderTrackId", {"senderStreamId"},
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  blink::FakeRTCRtpReceiverImpl receiver(
      "receiverTrackId", {"receiverStreamId"},
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  if (implementation_type ==
      blink::WebRTCRtpTransceiverImplementationType::kFullTransceiver) {
    transceiver = std::make_unique<blink::FakeRTCRtpTransceiverImpl>(
        base::nullopt, std::move(sender), std::move(receiver),
        false /* stopped */,
        webrtc::RtpTransceiverDirection::kSendOnly /* direction */,
        base::nullopt /* current_direction */);
  } else if (implementation_type ==
             blink::WebRTCRtpTransceiverImplementationType::kPlanBSenderOnly) {
    transceiver = std::make_unique<blink::RTCRtpSenderOnlyTransceiver>(
        std::make_unique<blink::FakeRTCRtpSenderImpl>(sender));
  } else {
    DCHECK_EQ(
        implementation_type,
        blink::WebRTCRtpTransceiverImplementationType::kPlanBReceiverOnly);
    transceiver = std::make_unique<blink::RTCRtpReceiverOnlyTransceiver>(
        std::make_unique<blink::FakeRTCRtpReceiverImpl>(receiver));
  }
  return transceiver;
}

namespace {

// TODO(https://crbug.com/868868): Move this into a separate file.
class MockPeerConnectionHandler : public RTCPeerConnectionHandler {
 public:
  MockPeerConnectionHandler()
      : RTCPeerConnectionHandler(
            &client_,
            &dependency_factory_,
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()) {}
  MOCK_METHOD0(CloseClientPeerConnection, void());

 private:
  blink::MockPeerConnectionDependencyFactory dependency_factory_;
  blink::MockWebRTCPeerConnectionHandlerClient client_;
};

class PeerConnectionTrackerTest : public ::testing::Test {
 public:
  void CreateTrackerWithMocks() {
    mock_host_.reset(new MockPeerConnectionTrackerHost());
    tracker_.reset(new PeerConnectionTracker(
        mock_host_->CreatePendingRemoteAndBind(),
        blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  }

  void CreateAndRegisterPeerConnectionHandler() {
    mock_handler_.reset(new MockPeerConnectionHandler());
    EXPECT_CALL(*mock_host_, AddPeerConnection(_));
    tracker_->RegisterPeerConnection(
        mock_handler_.get(),
        webrtc::PeerConnectionInterface::RTCConfiguration(),
        blink::WebMediaConstraints(), nullptr);
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<MockPeerConnectionTrackerHost> mock_host_;
  std::unique_ptr<PeerConnectionTracker> tracker_;
  std::unique_ptr<MockPeerConnectionHandler> mock_handler_;
};

}  // namespace

TEST_F(PeerConnectionTrackerTest, CreatingObject) {
  PeerConnectionTracker tracker(
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
}

TEST_F(PeerConnectionTrackerTest, TrackCreateOffer) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  // Note: blink::WebRTCOfferOptions is not mockable. So we can't write tests
  // for anything but a null options parameter.
  blink::WebRTCOfferOptions options(0, 0, false, false);
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(
                  _, "createOffer",
                  "options: {offerToReceiveVideo: 0, offerToReceiveAudio: 0, "
                  "voiceActivityDetection: false, iceRestart: false}"));
  tracker_->TrackCreateOffer(mock_handler_.get(), options);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest, OnSuspend) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  EXPECT_CALL(*mock_handler_, CloseClientPeerConnection());
  tracker_->OnSuspend();
}

TEST_F(PeerConnectionTrackerTest, AddTransceiverWithOptionalValuesPresent) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  blink::FakeRTCRtpTransceiverImpl transceiver(
      "midValue",
      blink::FakeRTCRtpSenderImpl(
          "senderTrackId", {"streamIdA", "streamIdB"},
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
      blink::FakeRTCRtpReceiverImpl(
          "receiverTrackId", {"streamIdC"},
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
      true /* stopped */,
      webrtc::RtpTransceiverDirection::kSendRecv /* direction */,
      webrtc::RtpTransceiverDirection::kInactive /* current_direction */);
  std::string update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, "transceiverAdded", _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackAddTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kAddTrack, transceiver,
      0u);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      "Caused by: addTrack\n"
      "\n"
      "getTransceivers()[0]:{\n"
      "  mid:'midValue',\n"
      "  sender:{\n"
      "    track:'senderTrackId',\n"
      "    streams:['streamIdA','streamIdB'],\n"
      "  },\n"
      "  receiver:{\n"
      "    track:'receiverTrackId',\n"
      "    streams:['streamIdC'],\n"
      "  },\n"
      "  stopped:true,\n"
      "  direction:'sendrecv',\n"
      "  currentDirection:'inactive',\n"
      "}",
      update_value);
}

TEST_F(PeerConnectionTrackerTest, AddTransceiverWithOptionalValuesNull) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  blink::FakeRTCRtpTransceiverImpl transceiver(
      base::nullopt,
      blink::FakeRTCRtpSenderImpl(
          base::nullopt, {},
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
      blink::FakeRTCRtpReceiverImpl(
          "receiverTrackId", {},
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
      false /* stopped */,
      webrtc::RtpTransceiverDirection::kInactive /* direction */,
      base::nullopt /* current_direction */);
  std::string update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, "transceiverAdded", _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackAddTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kAddTransceiver,
      transceiver, 1u);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      "Caused by: addTransceiver\n"
      "\n"
      "getTransceivers()[1]:{\n"
      "  mid:null,\n"
      "  sender:{\n"
      "    track:null,\n"
      "    streams:[],\n"
      "  },\n"
      "  receiver:{\n"
      "    track:'receiverTrackId',\n"
      "    streams:[],\n"
      "  },\n"
      "  stopped:false,\n"
      "  direction:'inactive',\n"
      "  currentDirection:null,\n"
      "}",
      update_value);
}

TEST_F(PeerConnectionTrackerTest, ModifyTransceiver) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto transceiver = CreateDefaultTransceiver(
      blink::WebRTCRtpTransceiverImplementationType::kFullTransceiver);
  std::string update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, "transceiverModified", _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackModifyTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetLocalDescription,
      *transceiver, 0u);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      "Caused by: setLocalDescription\n"
      "\n" +
          std::string(kDefaultTransceiverString),
      update_value);
}

TEST_F(PeerConnectionTrackerTest, RemoveTransceiver) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto transceiver = CreateDefaultTransceiver(
      blink::WebRTCRtpTransceiverImplementationType::kFullTransceiver);
  std::string update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, "transceiverRemoved", _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackRemoveTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kRemoveTrack,
      *transceiver, 0u);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      "Caused by: removeTrack\n"
      "\n" +
          std::string(kDefaultTransceiverString),
      update_value);
}

TEST_F(PeerConnectionTrackerTest, AddSender) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto sender_only = CreateDefaultTransceiver(
      blink::WebRTCRtpTransceiverImplementationType::kPlanBSenderOnly);
  std::string update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, "senderAdded", _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackAddTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetLocalDescription,
      *sender_only, 0u);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      "Caused by: setLocalDescription\n"
      "\n" +
          std::string(kDefaultSenderString),
      update_value);
}

TEST_F(PeerConnectionTrackerTest, ModifySender) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto sender_only = CreateDefaultTransceiver(
      blink::WebRTCRtpTransceiverImplementationType::kPlanBSenderOnly);
  std::string update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, "senderModified", _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackModifyTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription,
      *sender_only, 0u);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      "Caused by: setRemoteDescription\n"
      "\n" +
          std::string(kDefaultSenderString),
      update_value);
}

TEST_F(PeerConnectionTrackerTest, RemoveSender) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto sender_only = CreateDefaultTransceiver(
      blink::WebRTCRtpTransceiverImplementationType::kPlanBSenderOnly);
  std::string update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, "senderRemoved", _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackRemoveTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription,
      *sender_only, 0u);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      "Caused by: setRemoteDescription\n"
      "\n" +
          std::string(kDefaultSenderString),
      update_value);
}

TEST_F(PeerConnectionTrackerTest, AddReceiver) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto receiver_only = CreateDefaultTransceiver(
      blink::WebRTCRtpTransceiverImplementationType::kPlanBReceiverOnly);
  std::string update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, "receiverAdded", _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackAddTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription,
      *receiver_only, 0u);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      "Caused by: setRemoteDescription\n"
      "\n" +
          std::string(kDefaultReceiverString),
      update_value);
}

TEST_F(PeerConnectionTrackerTest, ModifyReceiver) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto receiver_only = CreateDefaultTransceiver(
      blink::WebRTCRtpTransceiverImplementationType::kPlanBReceiverOnly);
  std::string update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, "receiverModified", _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackModifyTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription,
      *receiver_only, 0u);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      "Caused by: setRemoteDescription\n"
      "\n" +
          std::string(kDefaultReceiverString),
      update_value);
}

TEST_F(PeerConnectionTrackerTest, RemoveReceiver) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto receiver_only = CreateDefaultTransceiver(
      blink::WebRTCRtpTransceiverImplementationType::kPlanBReceiverOnly);
  std::string update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, "receiverRemoved", _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackRemoveTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription,
      *receiver_only, 0u);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      "Caused by: setRemoteDescription\n"
      "\n" +
          std::string(kDefaultReceiverString),
      update_value);
}

// TODO(hta): Write tests for the other tracking functions.

}  // namespace
