// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/ack_message_handler.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"

AckMessageHandler::AckMessageHandler(
    ResponseCallbackHelper* response_callback_helper)
    : response_callback_helper_(response_callback_helper) {}

AckMessageHandler::~AckMessageHandler() = default;

void AckMessageHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  DCHECK(message.has_ack_message());
  chrome_browser_sharing::AckMessage* ack_message =
      message.mutable_ack_message();
  std::unique_ptr<chrome_browser_sharing::ResponseMessage> response;
  if (ack_message->has_response_message())
    response = base::WrapUnique(ack_message->release_response_message());

  response_callback_helper_->OnFCMAckReceived(
      ack_message->original_message_type(), ack_message->original_message_id(),
      std::move(response));

  std::move(done_callback).Run(/*response=*/nullptr);
}
