// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/webrtc/webrtc_message_handler.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/sharing/webrtc/sharing_service_host.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSharingServiceHost : public SharingServiceHost {
 public:
  MockSharingServiceHost()
      : SharingServiceHost(
            /*message_sender=*/nullptr,
            /*gcm_driver=*/nullptr,
            /*sync_prefs=*/nullptr,
            /*device_source=*/nullptr,
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  MockSharingServiceHost(const MockSharingServiceHost&) = delete;
  MockSharingServiceHost& operator=(const MockSharingServiceHost&) = delete;
  ~MockSharingServiceHost() override = default;

  MOCK_METHOD4(OnOfferReceived,
               void(const std::string& device_guid,
                    const chrome_browser_sharing::FCMChannelConfiguration&
                        fcm_configuration,
                    const std::string& offer,
                    base::OnceCallback<void(const std::string&)> callback));

  MOCK_METHOD3(
      OnIceCandidatesReceived,
      void(const std::string& device_guid,
           const chrome_browser_sharing::FCMChannelConfiguration&
               fcm_configuration,
           std::vector<sharing::mojom::IceCandidatePtr> ice_candidates));

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
};

MATCHER_P(ProtoEquals, message, "") {
  if (!arg)
    return false;

  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg->SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace

class WebRtcMessageHandlerTest : public testing::Test {
 public:
  WebRtcMessageHandlerTest()
      : webrtc_message_handler_(&mock_sharing_service_host_) {}
  WebRtcMessageHandlerTest(const WebRtcMessageHandlerTest&) = delete;
  WebRtcMessageHandlerTest& operator=(const WebRtcMessageHandlerTest&) = delete;
  ~WebRtcMessageHandlerTest() override = default;

 protected:
  testing::NiceMock<MockSharingServiceHost> mock_sharing_service_host_;
  WebRtcMessageHandler webrtc_message_handler_;
};

TEST_F(WebRtcMessageHandlerTest, IceCandidateMessageTest) {
  std::vector<sharing::mojom::IceCandidatePtr> expected_ice_candidates;
  expected_ice_candidates.push_back(
      sharing::mojom::IceCandidate::New("candidate0", "mid0", 0));
  expected_ice_candidates.push_back(
      sharing::mojom::IceCandidate::New("candidate1", "mid1", 1));

  chrome_browser_sharing::SharingMessage message;
  message.mutable_fcm_channel_configuration();
  for (const auto& ice_candidate : expected_ice_candidates) {
    chrome_browser_sharing::PeerConnectionIceCandidate* peer_candidate =
        message.mutable_peer_connection_ice_candidates_message()
            ->add_ice_candidates();
    peer_candidate->set_candidate(ice_candidate->candidate);
    peer_candidate->set_sdp_mid(ice_candidate->sdp_mid);
    peer_candidate->set_sdp_mline_index(ice_candidate->sdp_mline_index);
  }

  base::MockCallback<SharingMessageHandler::DoneCallback> mock_done_callback_;
  EXPECT_CALL(mock_sharing_service_host_,
              OnIceCandidatesReceived(testing::_, testing::_, testing::_))
      .WillOnce(
          [&](const std::string& device_guid,
              const chrome_browser_sharing::FCMChannelConfiguration&
                  fcm_configuration,
              std::vector<sharing::mojom::IceCandidatePtr> ice_candidates) {
            ASSERT_EQ(expected_ice_candidates, ice_candidates);
          });
  EXPECT_CALL(mock_done_callback_, Run(testing::Eq(nullptr)));

  webrtc_message_handler_.OnMessage(std::move(message),
                                    mock_done_callback_.Get());
}

TEST_F(WebRtcMessageHandlerTest, OfferMessageTest) {
  const std::string answer = "answer";
  chrome_browser_sharing::ResponseMessage expected_answer;
  expected_answer.mutable_peer_connection_answer_message_response()->set_sdp(
      answer);

  const std::string offer = "offer";
  chrome_browser_sharing::SharingMessage message;
  message.mutable_fcm_channel_configuration();
  message.mutable_peer_connection_offer_message()->set_sdp(offer);

  base::MockCallback<SharingMessageHandler::DoneCallback> mock_done_callback_;
  EXPECT_CALL(
      mock_sharing_service_host_,
      OnOfferReceived(testing::_, testing::_, testing::Eq(offer), testing::_))
      .WillOnce([&](const std::string& device_guid,
                    const chrome_browser_sharing::FCMChannelConfiguration&
                        fcm_configuration,
                    const std::string& offer,
                    base::OnceCallback<void(const std::string&)> callback) {
        std::move(callback).Run(answer);
      });
  EXPECT_CALL(mock_done_callback_, Run(ProtoEquals(expected_answer)));

  webrtc_message_handler_.OnMessage(std::move(message),
                                    mock_done_callback_.Get());
}
