// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_TEST_MOCK_WEBRTC_SIGNALING_MESSENGER_H_
#define CHROME_SERVICES_SHARING_NEARBY_TEST_MOCK_WEBRTC_SIGNALING_MESSENGER_H_

#include "chrome/services/sharing/public/mojom/webrtc_signaling_messenger.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace sharing {

class MockWebRtcSignalingMessenger : public mojom::WebRtcSignalingMessenger {
 public:
  MockWebRtcSignalingMessenger();
  ~MockWebRtcSignalingMessenger() override;

  // sharing::mojom::WebRtcSignalingMessenger:
  void SendMessage(const std::string& self_id,
                   const std::string& peer_id,
                   const std::string& message,
                   SendMessageCallback callback) override;
  void StartReceivingMessages(
      const std::string& self_id,
      mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
          incoming_messages_listener,
      StartReceivingMessagesCallback callback) override;
  void StopReceivingMessages() override;

  mojo::Receiver<mojom::WebRtcSignalingMessenger> messenger{this};
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_NEARBY_TEST_MOCK_WEBRTC_SIGNALING_MESSENGER_H_
