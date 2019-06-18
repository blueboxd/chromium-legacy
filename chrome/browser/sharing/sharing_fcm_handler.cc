// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_handler.h"

#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "components/gcm_driver/gcm_driver.h"

namespace {
const char kSharingFCMAppID[] = "com.google.chrome.sharing.fcm";
}  // namespace

SharingFCMHandler::SharingFCMHandler(gcm::GCMDriver* gcm_driver,
                                     SharingFCMSender* sharing_fcm_sender)
    : gcm_driver_(gcm_driver),
      sharing_fcm_sender_(sharing_fcm_sender),
      weak_ptr_factory_(this) {
  StartListening();
}

SharingFCMHandler::~SharingFCMHandler() {
  StopListening();
}

void SharingFCMHandler::StartListening() {
  gcm_driver_->AddAppHandler(kSharingFCMAppID, this);
}

void SharingFCMHandler::StopListening() {
  gcm_driver_->RemoveAppHandler(kSharingFCMAppID);
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
  NOTIMPLEMENTED();
}

void SharingFCMHandler::ShutdownHandler() {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
}

bool SharingFCMHandler::SendAckMessage(
    const std::string& original_sender_guid,
    const std::string& original_message_id) const {
  SharingMessage ack_message;
  ack_message.mutable_ack_message()->set_original_message_id(
      original_message_id);
  return sharing_fcm_sender_->SendMessage(original_sender_guid, ack_message);
}
