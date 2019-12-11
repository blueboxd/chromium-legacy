// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_client.h"

#include "base/logging.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_receiver_platform.h"

using testing::_;

namespace blink {

MockRTCPeerConnectionHandlerClient::MockRTCPeerConnectionHandlerClient() {
  ON_CALL(*this, DidGenerateICECandidate(_))
      .WillByDefault(testing::Invoke(
          this,
          &MockRTCPeerConnectionHandlerClient::didGenerateICECandidateWorker));
  ON_CALL(*this, DidAddReceiverPlanBForMock(_))
      .WillByDefault(testing::Invoke(
          this, &MockRTCPeerConnectionHandlerClient::didAddReceiverWorker));
  ON_CALL(*this, DidRemoveReceiverPlanBForMock(_))
      .WillByDefault(testing::Invoke(
          this, &MockRTCPeerConnectionHandlerClient::didRemoveReceiverWorker));
}

MockRTCPeerConnectionHandlerClient::~MockRTCPeerConnectionHandlerClient() {}

void MockRTCPeerConnectionHandlerClient::didGenerateICECandidateWorker(
    RTCIceCandidatePlatform* candidate) {
  candidate_sdp_ = candidate->Candidate().Utf8();
  candidate_mline_index_ = candidate->SdpMLineIndex();
  candidate_mid_ = candidate->SdpMid().Utf8();
}

void MockRTCPeerConnectionHandlerClient::didAddReceiverWorker(
    std::unique_ptr<RTCRtpReceiverPlatform>* web_rtp_receiver) {
  WebVector<String> stream_ids = (*web_rtp_receiver)->StreamIds();
  DCHECK_EQ(1u, stream_ids.size());
  remote_stream_id_ = stream_ids[0];
}

void MockRTCPeerConnectionHandlerClient::didRemoveReceiverWorker(
    std::unique_ptr<RTCRtpReceiverPlatform>* web_rtp_receiver) {
  remote_stream_id_ = String();
}

}  // namespace blink
