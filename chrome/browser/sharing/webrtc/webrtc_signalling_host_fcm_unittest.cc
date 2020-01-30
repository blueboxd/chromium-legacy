// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/webrtc/webrtc_signalling_host_fcm.h"

#include "base/test/bind_test_util.h"
#include "chrome/browser/sharing/fake_device_info.h"
#include "chrome/browser/sharing/mock_sharing_message_sender.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "chrome/services/sharing/public/mojom/webrtc.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSignallingService : public sharing::mojom::SignallingReceiver {
 public:
  MockSignallingService() = default;
  MockSignallingService(const MockSignallingService&) = delete;
  MockSignallingService& operator=(const MockSignallingService&) = delete;
  ~MockSignallingService() override = default;

  // sharing::mojom::SignallingReceiver:
  MOCK_METHOD2(OnOfferReceived,
               void(const std::string&, OnOfferReceivedCallback));
  MOCK_METHOD1(OnIceCandidatesReceived,
               void(std::vector<sharing::mojom::IceCandidatePtr>));

  mojo::Remote<sharing::mojom::SignallingSender> signalling_sender;
  mojo::Receiver<sharing::mojom::SignallingReceiver> signalling_receiver{this};
};

class WebRtcSignallingHostFCMTest : public testing::Test {
 public:
  WebRtcSignallingHostFCMTest() = default;

  void RespondWith(
      SharingSendMessageResult result,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
    next_result_ = result;
    next_response_ = std::move(response);
    EXPECT_CALL(message_sender_,
                SendMessageToDevice(testing::_, testing::_, testing::_,
                                    SharingMessageSender::DelegateType::kFCM,
                                    testing::_))
        .WillOnce(testing::Invoke(
            [&](const syncer::DeviceInfo& device_info,
                base::TimeDelta response_timeout,
                chrome_browser_sharing::SharingMessage message,
                SharingMessageSender::DelegateType delegate_type,
                SharingMessageSender::ResponseCallback callback) {
              std::move(callback).Run(next_result_, std::move(next_response_));
            }));
  }

  std::string SendOfferSync(const std::string& offer) {
    std::string result;
    base::RunLoop run_loop;
    signalling_host_.SendOffer(
        offer, base::BindLambdaForTesting([&](const std::string& response) {
          result = response;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  MockSharingMessageSender message_sender_;
  MockSignallingService signalling_service_;
  WebRtcSignallingHostFCM signalling_host_{
      signalling_service_.signalling_sender.BindNewPipeAndPassReceiver(),
      signalling_service_.signalling_receiver.BindNewPipeAndPassRemote(),
      &message_sender_, CreateFakeDeviceInfo("id", "name")};

 private:
  SharingSendMessageResult next_result_;
  std::unique_ptr<chrome_browser_sharing::ResponseMessage> next_response_;
};

}  // namespace

TEST_F(WebRtcSignallingHostFCMTest, SendOffer_Success) {
  auto answer = std::make_unique<chrome_browser_sharing::ResponseMessage>();
  answer->mutable_peer_connection_answer_message_response()->set_sdp("answer");

  RespondWith(SharingSendMessageResult::kSuccessful, std::move(answer));

  EXPECT_EQ("answer", SendOfferSync("offer"));
}

TEST_F(WebRtcSignallingHostFCMTest, SendOffer_NoAnswer) {
  auto answer = std::make_unique<chrome_browser_sharing::ResponseMessage>();

  RespondWith(SharingSendMessageResult::kSuccessful, std::move(answer));

  EXPECT_EQ("", SendOfferSync("offer"));
}

TEST_F(WebRtcSignallingHostFCMTest, SendOffer_NoResponse) {
  RespondWith(SharingSendMessageResult::kSuccessful, nullptr);

  EXPECT_EQ("", SendOfferSync("offer"));
}

TEST_F(WebRtcSignallingHostFCMTest, SendIceCandidates_Empty) {
  EXPECT_CALL(
      message_sender_,
      SendMessageToDevice(testing::_, testing::_, testing::_,
                          SharingMessageSender::DelegateType::kFCM, testing::_))
      .Times(0);

  std::vector<sharing::mojom::IceCandidatePtr> ice_candidates;
  signalling_host_.SendIceCandidates(std::move(ice_candidates));
}

TEST_F(WebRtcSignallingHostFCMTest, SendIceCandidates_Success) {
  chrome_browser_sharing::PeerConnectionIceCandidate ice_candidate;
  ice_candidate.set_candidate("candidate");
  ice_candidate.set_sdp_mid("sdp_mid");
  ice_candidate.set_sdp_mline_index(3);

  base::RunLoop run_loop;
  chrome_browser_sharing::SharingMessage sent_message;
  EXPECT_CALL(
      message_sender_,
      SendMessageToDevice(testing::_, testing::_, testing::_,
                          SharingMessageSender::DelegateType::kFCM, testing::_))
      .WillOnce(
          testing::Invoke([&](const syncer::DeviceInfo& device_info,
                              base::TimeDelta response_timeout,
                              chrome_browser_sharing::SharingMessage message,
                              SharingMessageSender::DelegateType delegate_type,
                              SharingMessageSender::ResponseCallback callback) {
            sent_message = std::move(message);
            std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                    nullptr);
            run_loop.Quit();
          }));

  std::vector<sharing::mojom::IceCandidatePtr> ice_candidates;
  ice_candidates.push_back(sharing::mojom::IceCandidate::New(
      ice_candidate.candidate(), ice_candidate.sdp_mid(),
      ice_candidate.sdp_mline_index()));
  signalling_host_.SendIceCandidates(std::move(ice_candidates));
  run_loop.Run();

  ASSERT_TRUE(sent_message.has_peer_connection_ice_candidates_message());
  const auto& ice_message =
      sent_message.peer_connection_ice_candidates_message();
  ASSERT_EQ(1, ice_message.ice_candidates_size());
  const auto& ice_candidate_sent = ice_message.ice_candidates(0);

  EXPECT_EQ(ice_candidate.candidate(), ice_candidate_sent.candidate());
  EXPECT_EQ(ice_candidate.sdp_mid(), ice_candidate_sent.sdp_mid());
  EXPECT_EQ(ice_candidate.sdp_mline_index(),
            ice_candidate_sent.sdp_mline_index());
}

TEST_F(WebRtcSignallingHostFCMTest, OnOfferReceived) {
  EXPECT_CALL(signalling_service_, OnOfferReceived("offer", testing::_))
      .WillOnce(testing::Invoke(
          [&](const std::string& offer,
              base::OnceCallback<void(const std::string&)> callback) {
            EXPECT_EQ("offer", offer);
            std::move(callback).Run("answer");
          }));

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const std::string& answer) {
    EXPECT_EQ("answer", answer);
    run_loop.Quit();
  });

  signalling_host_.OnOfferReceived("offer", std::move(callback));
  run_loop.Run();
}

TEST_F(WebRtcSignallingHostFCMTest, OnIceCandidatesReceived) {
  base::RunLoop run_loop;
  EXPECT_CALL(signalling_service_, OnIceCandidatesReceived(testing::_))
      .WillOnce(testing::Invoke(
          [&](std::vector<sharing::mojom::IceCandidatePtr> ice_candidates) {
            EXPECT_EQ(1u, ice_candidates.size());
            run_loop.Quit();
          }));

  std::vector<sharing::mojom::IceCandidatePtr> ice_candidates;
  ice_candidates.push_back(sharing::mojom::IceCandidate::New());
  signalling_host_.OnIceCandidatesReceived(std::move(ice_candidates));
  run_loop.Run();
}
