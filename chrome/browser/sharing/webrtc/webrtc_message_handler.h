// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_WEBRTC_WEBRTC_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_WEBRTC_WEBRTC_MESSAGE_HANDLER_H_

#include <string>

#include "chrome/browser/sharing/proto/peer_connection_messages.pb.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "components/sync_device_info/device_info.h"

class SharingServiceHost;

// Handles WebRTC specific Sharing messages received via FCM that implement the
// signalling part of the WebRTC communication between two browser instances.
// Relays incoming messages to the |sharing_service_host|.
// This object is owned by the SharingHandlerRegistryImpl which is owned by the
// SharingService KeyedService.
class WebRtcMessageHandler : public SharingMessageHandler {
 public:
  explicit WebRtcMessageHandler(SharingServiceHost* sharing_service_host);
  WebRtcMessageHandler(const WebRtcMessageHandler&) = delete;
  WebRtcMessageHandler& operator=(const WebRtcMessageHandler&) = delete;
  ~WebRtcMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(chrome_browser_sharing::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 private:
  void HandleOfferMessage(
      const std::string& sender_guid,
      const chrome_browser_sharing::FCMChannelConfiguration& fcm_configuration,
      const chrome_browser_sharing::PeerConnectionOfferMessage& message,
      SharingMessageHandler::DoneCallback done_callback);

  static void ReplyWithAnswer(SharingMessageHandler::DoneCallback done_callback,
                              const std::string& answer);

  void HandleIceCandidatesMessage(
      const std::string& sender_guid,
      const chrome_browser_sharing::FCMChannelConfiguration& fcm_configuration,
      const chrome_browser_sharing::PeerConnectionIceCandidatesMessage& message,
      SharingMessageHandler::DoneCallback done_callback);

  // Owned by the SharingService KeyedService and must outlive |this|.
  SharingServiceHost* sharing_service_host_;
};

#endif  // CHROME_BROWSER_SHARING_WEBRTC_WEBRTC_MESSAGE_HANDLER_H_
