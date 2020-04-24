// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/webrtc/sharing_webrtc_connection_host.h"

#include <memory>

#include "base/callback.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_handler_registry.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/webrtc/webrtc_signalling_host_fcm.h"
#include "chrome/services/sharing/public/cpp/sharing_webrtc_metrics.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/gcm_driver.h"

namespace {

bool IsValidSharingWebRtcPayloadCase(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  // WebRTC signalling messages should only be received via FCM.
  return payload_case != chrome_browser_sharing::SharingMessage::
                             kPeerConnectionOfferMessage &&
         payload_case != chrome_browser_sharing::SharingMessage::
                             kPeerConnectionIceCandidatesMessage;
}

}  // namespace

SharingWebRtcConnectionHost::SharingWebRtcConnectionHost(
    std::unique_ptr<WebRtcSignallingHostFCM> signalling_host,
    SharingHandlerRegistry* handler_registry,
    gcm::GCMDriver* gcm_driver,
    EncryptionInfo encryption_info,
    base::OnceClosure on_closed,
    mojo::PendingReceiver<sharing::mojom::SharingWebRtcConnectionDelegate>
        delegate,
    mojo::PendingRemote<sharing::mojom::SharingWebRtcConnection> connection,
    mojo::PendingReceiver<network::mojom::P2PTrustedSocketManagerClient>
        socket_manager_client,
    mojo::PendingRemote<network::mojom::P2PTrustedSocketManager> socket_manager)
    : signalling_host_(std::move(signalling_host)),
      handler_registry_(handler_registry),
      gcm_driver_(gcm_driver),
      encryption_info_(std::move(encryption_info)),
      on_closed_(std::move(on_closed)),
      delegate_(this, std::move(delegate)),
      connection_(std::move(connection)),
      socket_manager_client_(this, std::move(socket_manager_client)),
      socket_manager_(std::move(socket_manager)),
      timeout_state_(sharing::WebRtcTimeoutState::kConnecting),
      timeout_timer_(FROM_HERE,
                     kSharingWebRtcTimeout,
                     this,
                     &SharingWebRtcConnectionHost::OnConnectionTimeout) {
  delegate_.set_disconnect_handler(
      base::BindOnce(&SharingWebRtcConnectionHost::OnConnectionClosing,
                     weak_ptr_factory_.GetWeakPtr()));
  connection_.set_disconnect_handler(
      base::BindOnce(&SharingWebRtcConnectionHost::OnConnectionClosing,
                     weak_ptr_factory_.GetWeakPtr()));

  socket_manager_client_.set_disconnect_handler(
      base::BindOnce(&SharingWebRtcConnectionHost::OnConnectionClosed,
                     weak_ptr_factory_.GetWeakPtr()));
  socket_manager_.set_disconnect_handler(
      base::BindOnce(&SharingWebRtcConnectionHost::OnConnectionClosed,
                     weak_ptr_factory_.GetWeakPtr()));
  timeout_timer_.Reset();
}

SharingWebRtcConnectionHost::~SharingWebRtcConnectionHost() = default;

void SharingWebRtcConnectionHost::OnMessageReceived(
    const std::vector<uint8_t>& message) {
  // TODO(crbug.com/1045408): hook this up to a fuzzer.
  // TODO(crbug.com/1046333): consider a different mojo interface to avoid copy
  std::string raw_data(message.data(), message.data() + message.size());
  gcm_driver_->DecryptMessage(
      kSharingFCMAppID, encryption_info_.authorized_entity, raw_data,
      base::BindOnce(&SharingWebRtcConnectionHost::OnMessageDecrypted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharingWebRtcConnectionHost::OnMessageDecrypted(
    gcm::GCMDecryptionResult result,
    std::string message) {
  chrome_browser_sharing::WebRtcMessage sharing_message;
  if (result != gcm::GCMDecryptionResult::DECRYPTED_DRAFT_08) {
    LogSharingWebRtcOnMessageReceivedResult(
        sharing::WebRtcOnMessageReceivedResult::kDecryptionFailed);
    return;
  }

  if (!sharing_message.ParseFromString(message)) {
    LogSharingWebRtcOnMessageReceivedResult(
        sharing::WebRtcOnMessageReceivedResult::kParseFailed);
    return;
  }

  auto payload_case = sharing_message.message().payload_case();
  if (!IsValidSharingWebRtcPayloadCase(payload_case)) {
    LogSharingWebRtcOnMessageReceivedResult(
        sharing::WebRtcOnMessageReceivedResult::kInvalidPayload);
    return;
  }

  auto* handler = handler_registry_->GetSharingHandler(payload_case);
  if (!handler) {
    LogSharingWebRtcOnMessageReceivedResult(
        sharing::WebRtcOnMessageReceivedResult::kHandlerNotFound);
    return;
  }

  LogSharingWebRtcOnMessageReceivedResult(
      sharing::WebRtcOnMessageReceivedResult::kSuccess);

  timeout_state_ = sharing::WebRtcTimeoutState::kMessageReceived;
  timeout_timer_.Reset();

  std::string original_message_id = sharing_message.message_guid();
  chrome_browser_sharing::MessageType original_message_type =
      SharingPayloadCaseToMessageType(payload_case);

  handler->OnMessage(
      std::move(sharing_message.message()),
      base::BindOnce(&SharingWebRtcConnectionHost::OnMessageHandled,
                     weak_ptr_factory_.GetWeakPtr(), original_message_id,
                     original_message_type));
}

void SharingWebRtcConnectionHost::OnMessageHandled(
    const std::string& original_message_id,
    chrome_browser_sharing::MessageType original_message_type,
    std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
  if (original_message_type ==
      chrome_browser_sharing::MessageType::ACK_MESSAGE) {
    OnConnectionClosing();
    return;
  }

  chrome_browser_sharing::WebRtcMessage message;
  auto* sharing_message = message.mutable_message();
  auto* ack_message = sharing_message->mutable_ack_message();
  ack_message->set_original_message_id(original_message_id);
  if (response)
    ack_message->set_allocated_response_message(response.release());

  SendMessage(std::move(message),
              base::BindOnce(&SharingWebRtcConnectionHost::OnAckSent,
                             weak_ptr_factory_.GetWeakPtr()));
}

void SharingWebRtcConnectionHost::OnAckSent(
    sharing::mojom::SendMessageResult result) {
  OnConnectionClosing();
}

void SharingWebRtcConnectionHost::OnConnectionClosing() {
  timeout_state_ = sharing::WebRtcTimeoutState::kDisconnecting;
  timeout_timer_.Reset();
  connection_.reset();
  delegate_.reset();
}

void SharingWebRtcConnectionHost::OnConnectionClosed() {
  if (on_closed_)
    std::move(on_closed_).Run();
}

void SharingWebRtcConnectionHost::OnConnectionTimeout() {
  sharing::LogWebRtcTimeout(timeout_state_);
  OnConnectionClosing();
  OnConnectionClosed();
}

void SharingWebRtcConnectionHost::SendMessage(
    chrome_browser_sharing::WebRtcMessage message,
    sharing::mojom::SharingWebRtcConnection::SendMessageCallback callback) {
  std::string serialized_message;
  if (!message.SerializeToString(&serialized_message)) {
    std::move(callback).Run(sharing::mojom::SendMessageResult::kError);
    return;
  }

  gcm_driver_->EncryptMessage(
      kSharingFCMAppID, encryption_info_.authorized_entity,
      encryption_info_.p256dh, encryption_info_.auth_secret, serialized_message,
      base::BindOnce(&SharingWebRtcConnectionHost::OnMessageEncrypted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SharingWebRtcConnectionHost::OnMessageEncrypted(
    sharing::mojom::SharingWebRtcConnection::SendMessageCallback callback,
    gcm::GCMEncryptionResult result,
    std::string message) {
  if (result != gcm::GCMEncryptionResult::ENCRYPTED_DRAFT_08) {
    std::move(callback).Run(sharing::mojom::SendMessageResult::kError);
    return;
  }

  timeout_state_ = sharing::WebRtcTimeoutState::kMessageSent;
  timeout_timer_.Reset();

  // TODO(crbug.com/1045406): encrypt |serialized_message|.
  // TODO(crbug.com/1046333): consider a different mojo interface to avoid copy
  std::vector<uint8_t> serialized_message(message.begin(), message.end());
  connection_->SendMessage(serialized_message, std::move(callback));
}

void SharingWebRtcConnectionHost::OnOfferReceived(
    const std::string& offer,
    base::OnceCallback<void(const std::string&)> callback) {
  signalling_host_->OnOfferReceived(offer, std::move(callback));
}

void SharingWebRtcConnectionHost::OnIceCandidatesReceived(
    std::vector<sharing::mojom::IceCandidatePtr> ice_candidates) {
  signalling_host_->OnIceCandidatesReceived(std::move(ice_candidates));
}

void SharingWebRtcConnectionHost::InvalidSocketPortRangeRequested() {
  // TODO(crbug.com/1021984): Add metrics for this.
}

void SharingWebRtcConnectionHost::DumpPacket(
    const std::vector<uint8_t>& packet_header,
    uint64_t packet_length,
    bool incoming) {
  NOTIMPLEMENTED();
}
