// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/ack_message_handler.h"

#include "chrome/browser/sharing/proto/sharing_message.pb.h"

AckMessageHandler::AckMessageHandler() = default;

AckMessageHandler::~AckMessageHandler() = default;

void AckMessageHandler::OnMessage(
    const chrome_browser_sharing::SharingMessage& message) {
  for (AckMessageObserver& observer : observers_)
    observer.OnAckReceived(message.ack_message().original_message_id());
}

void AckMessageHandler::AddObserver(AckMessageObserver* observer) {
  observers_.AddObserver(observer);
}

void AckMessageHandler::RemoveObserver(AckMessageObserver* observer) {
  observers_.RemoveObserver(observer);
}
