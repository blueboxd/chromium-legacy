// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/webrtc/incoming_messages_listener.h"

namespace sharing {

IncomingMessagesListener::IncomingMessagesListener(
    base::RepeatingCallback<void(const std::string& message)> listener)
    : listener_(listener) {}
IncomingMessagesListener::~IncomingMessagesListener() = default;

void IncomingMessagesListener::OnMessage(const std::string& message) {
  listener_.Run(message);
}

}  // namespace sharing
