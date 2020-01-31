// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/webrtc/webrtc_message_handler.h"

#include <memory>
#include <utility>

#include "chrome/browser/sharing/webrtc/sharing_service_host.h"

namespace {

std::vector<sharing::mojom::IceCandidatePtr> GetIceCandidates(
    const chrome_browser_sharing::PeerConnectionIceCandidatesMessage& message) {
  std::vector<sharing::mojom::IceCandidatePtr> ice_candidates;
  for (const auto& candidate : message.ice_candidates()) {
    ice_candidates.push_back(sharing::mojom::IceCandidate::New(
        candidate.candidate(), candidate.sdp_mid(),
        candidate.sdp_mline_index()));
  }
  return ice_candidates;
}

}  // namespace

WebRtcMessageHandler::WebRtcMessageHandler(
    SharingServiceHost* sharing_service_host)
    : sharing_service_host_(sharing_service_host) {
  DCHECK(sharing_service_host_);
}

WebRtcMessageHandler::~WebRtcMessageHandler() = default;

void WebRtcMessageHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  // Do not accept the message if we did not get a valid target info.
  if (!message.has_fcm_channel_configuration()) {
    // TODO(crbug.com/1021984): replace this with UMA metrics
    LOG(ERROR) << "Discarding message without fcm channel info";
    return;
  }

  if (message.has_peer_connection_offer_message()) {
    HandleOfferMessage(
        message.sender_guid(), message.fcm_channel_configuration(),
        message.peer_connection_offer_message(), std::move(done_callback));
    return;
  }

  if (message.has_peer_connection_ice_candidates_message()) {
    HandleIceCandidatesMessage(message.sender_guid(),
                               message.fcm_channel_configuration(),
                               message.peer_connection_ice_candidates_message(),
                               std::move(done_callback));
    return;
  }

  NOTREACHED();
}

void WebRtcMessageHandler::HandleOfferMessage(
    const std::string& sender_guid,
    const chrome_browser_sharing::FCMChannelConfiguration& fcm_configuration,
    const chrome_browser_sharing::PeerConnectionOfferMessage& message,
    SharingMessageHandler::DoneCallback done_callback) {
  sharing_service_host_->OnOfferReceived(
      sender_guid, fcm_configuration, message.sdp(),
      base::BindOnce(&ReplyWithAnswer, std::move(done_callback)));
}

// This is called from the sandboxed process with the received answer.
void WebRtcMessageHandler::ReplyWithAnswer(
    SharingMessageHandler::DoneCallback done_callback,
    const std::string& answer) {
  auto response = std::make_unique<chrome_browser_sharing::ResponseMessage>();
  response->mutable_peer_connection_answer_message_response()->set_sdp(answer);
  std::move(done_callback).Run(std::move(response));
}

void WebRtcMessageHandler::HandleIceCandidatesMessage(
    const std::string& sender_guid,
    const chrome_browser_sharing::FCMChannelConfiguration& fcm_configuration,
    const chrome_browser_sharing::PeerConnectionIceCandidatesMessage& message,
    SharingMessageHandler::DoneCallback done_callback) {
  sharing_service_host_->OnIceCandidatesReceived(sender_guid, fcm_configuration,
                                                 GetIceCandidates(message));
  std::move(done_callback).Run(/*response=*/nullptr);
}
