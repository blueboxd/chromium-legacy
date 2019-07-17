// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_handler.h"

#include "chrome/browser/sharing/fcm_constants.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "components/gcm_driver/gcm_driver.h"

namespace {
// Time to live for ACK messages.
const int kAckTimeToLiveMinutes = 30;
}  // namespace

SharingFCMHandler::SharingFCMHandler(gcm::GCMDriver* gcm_driver,
                                     SharingFCMSender* sharing_fcm_sender)
    : gcm_driver_(gcm_driver), sharing_fcm_sender_(sharing_fcm_sender) {}

SharingFCMHandler::~SharingFCMHandler() {
  StopListening();
}

void SharingFCMHandler::StartListening() {
  if (!is_listening_) {
    gcm_driver_->AddAppHandler(kSharingFCMAppID, this);
    is_listening_ = true;
  }
}

void SharingFCMHandler::StopListening() {
  if (is_listening_) {
    gcm_driver_->RemoveAppHandler(kSharingFCMAppID);
    is_listening_ = false;
  }
}

void SharingFCMHandler::AddSharingHandler(
    const SharingMessage::PayloadCase& payload_case,
    SharingMessageHandler* handler) {
  DCHECK(handler) << "Received request to add null handler";
  DCHECK(payload_case != SharingMessage::PAYLOAD_NOT_SET)
      << "Incorrect payload type specified for handler";
  DCHECK(!sharing_handlers_.count(payload_case)) << "Handler already exists";

  sharing_handlers_[payload_case] = handler;
}

void SharingFCMHandler::RemoveSharingHandler(
    const SharingMessage::PayloadCase& payload_case) {
  sharing_handlers_.erase(payload_case);
}

void SharingFCMHandler::OnMessagesDeleted(const std::string& app_id) {
  // TODO: Handle message deleted from the server.
}

void SharingFCMHandler::ShutdownHandler() {
  is_listening_ = false;
}

void SharingFCMHandler::OnMessage(const std::string& app_id,
                                  const gcm::IncomingMessage& message) {
  SharingMessage sharing_message;
  if (!sharing_message.ParseFromString(message.raw_data)) {
    LOG(ERROR) << "Failed to parse incoming message with id : "
               << message.message_id;
    return;
  }
  DCHECK(sharing_message.payload_case() != SharingMessage::PAYLOAD_NOT_SET)
      << "No payload set in SharingMessage received";

  LogSharingMessageReceived(sharing_message.payload_case());

  auto it = sharing_handlers_.find(sharing_message.payload_case());
  if (it == sharing_handlers_.end()) {
    LOG(ERROR) << "No handler found for payload : "
               << sharing_message.payload_case();
  } else {
    it->second->OnMessage(sharing_message);

    if (sharing_message.payload_case() != SharingMessage::kAckMessage)
      SendAckMessage(sharing_message.sender_guid(), message.message_id);
  }
}

void SharingFCMHandler::OnSendAcknowledged(const std::string& app_id,
                                           const std::string& message_id) {
  NOTIMPLEMENTED();
}

void SharingFCMHandler::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  NOTIMPLEMENTED();
}

void SharingFCMHandler::OnStoreReset() {
  // TODO: Handle GCM store reset.
}

void SharingFCMHandler::SendAckMessage(const std::string& original_sender_guid,
                                       const std::string& original_message_id) {
  SharingMessage ack_message;
  ack_message.mutable_ack_message()->set_original_message_id(
      original_message_id);
  sharing_fcm_sender_->SendMessageToDevice(
      original_sender_guid, base::TimeDelta::FromMinutes(kAckTimeToLiveMinutes),
      std::move(ack_message),
      base::BindOnce(&SharingFCMHandler::OnAckMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), original_message_id));
}

void SharingFCMHandler::OnAckMessageSent(
    const std::string& original_message_id,
    base::Optional<std::string> message_id) {
  if (!message_id) {
    LOG(ERROR) << "Failed to send ack mesage for " << original_message_id;
  }
}
