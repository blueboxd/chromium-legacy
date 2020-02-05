// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/webrtc/webrtc_signalling_host_fcm.h"

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"

WebRtcSignallingHostFCM::WebRtcSignallingHostFCM(
    mojo::PendingReceiver<sharing::mojom::SignallingSender> signalling_sender,
    mojo::PendingRemote<sharing::mojom::SignallingReceiver> signalling_receiver,
    SharingMessageSender* message_sender,
    std::unique_ptr<syncer::DeviceInfo> device_info)
    : message_sender_(message_sender),
      device_info_(std::move(device_info)),
      signalling_sender_(this, std::move(signalling_sender)),
      signalling_receiver_(std::move(signalling_receiver)) {
  DCHECK(device_info_);
}

WebRtcSignallingHostFCM::~WebRtcSignallingHostFCM() = default;

void WebRtcSignallingHostFCM::SendOffer(const std::string& offer,
                                        SendOfferCallback callback) {
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_peer_connection_offer_message()->set_sdp(offer);
  message_sender_->SendMessageToDevice(
      *device_info_,
      base::TimeDelta::FromSeconds(kSharingMessageTTLSeconds.Get()),
      std::move(sharing_message), SharingMessageSender::DelegateType::kFCM,
      base::BindOnce(&WebRtcSignallingHostFCM::OnOfferSent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebRtcSignallingHostFCM::OnOfferSent(
    SendOfferCallback callback,
    SharingSendMessageResult result,
    std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
  if (result != SharingSendMessageResult::kSuccessful || !response ||
      !response->has_peer_connection_answer_message_response()) {
    std::move(callback).Run(std::string());
    return;
  }

  std::move(callback).Run(
      response->peer_connection_answer_message_response().sdp());
}

void WebRtcSignallingHostFCM::SendIceCandidates(
    std::vector<sharing::mojom::IceCandidatePtr> ice_candidates) {
  if (ice_candidates.empty())
    return;

  chrome_browser_sharing::SharingMessage sharing_message;
  auto* mutable_ice_candidates =
      sharing_message.mutable_peer_connection_ice_candidates_message()
          ->mutable_ice_candidates();

  for (const auto& ice_candidate : ice_candidates) {
    chrome_browser_sharing::PeerConnectionIceCandidate ice_candidate_entry;
    ice_candidate_entry.set_candidate(ice_candidate->candidate);
    ice_candidate_entry.set_sdp_mid(ice_candidate->sdp_mid);
    ice_candidate_entry.set_sdp_mline_index(ice_candidate->sdp_mline_index);
    mutable_ice_candidates->Add(std::move(ice_candidate_entry));
  }

  message_sender_->SendMessageToDevice(
      *device_info_,
      base::TimeDelta::FromSeconds(kSharingMessageTTLSeconds.Get()),
      std::move(sharing_message), SharingMessageSender::DelegateType::kFCM,
      base::DoNothing());
}

void WebRtcSignallingHostFCM::OnOfferReceived(
    const std::string& offer,
    base::OnceCallback<void(const std::string&)> callback) {
  signalling_receiver_->OnOfferReceived(offer, std::move(callback));
}

void WebRtcSignallingHostFCM::OnIceCandidatesReceived(
    std::vector<sharing::mojom::IceCandidatePtr> ice_candidates) {
  signalling_receiver_->OnIceCandidatesReceived(std::move(ice_candidates));
}
