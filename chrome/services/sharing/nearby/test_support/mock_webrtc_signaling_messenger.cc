// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/test_support/mock_webrtc_signaling_messenger.h"

namespace sharing {

MockWebRtcSignalingMessenger::MockWebRtcSignalingMessenger() = default;

MockWebRtcSignalingMessenger::~MockWebRtcSignalingMessenger() = default;

void MockWebRtcSignalingMessenger::SendMessage(const std::string& self_id,
                                               const std::string& peer_id,
                                               const std::string& message,
                                               SendMessageCallback callback) {}

void MockWebRtcSignalingMessenger::StartReceivingMessages(
    const std::string& self_id,
    mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
        incoming_messages_listener,
    StartReceivingMessagesCallback callback) {}

void MockWebRtcSignalingMessenger::StopReceivingMessages() {}

}  // namespace sharing
