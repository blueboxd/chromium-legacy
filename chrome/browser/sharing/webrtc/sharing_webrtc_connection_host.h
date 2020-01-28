// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_WEBRTC_SHARING_WEBRTC_CONNECTION_HOST_H_
#define CHROME_BROWSER_SHARING_WEBRTC_SHARING_WEBRTC_CONNECTION_HOST_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "chrome/services/sharing/public/cpp/sharing_webrtc_metrics.h"
#include "chrome/services/sharing/public/mojom/webrtc.mojom.h"
#include "components/sync_device_info/device_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"

class SharingHandlerRegistry;
class WebRtcSignallingHostFCM;

// Host endpoint of a SharingWebRtcConnection. This class runs in the browser
// process and communicates with the SharingWebRtcConnection in a sandboxed
// process. This bridges Sharing messages via a WebRTC connection to another
// browser instance running on a remote device.
class SharingWebRtcConnectionHost
    : public sharing::mojom::SharingWebRtcConnectionDelegate,
      public network::mojom::P2PTrustedSocketManagerClient {
 public:
  SharingWebRtcConnectionHost(
      std::unique_ptr<WebRtcSignallingHostFCM> signalling_host,
      SharingHandlerRegistry* handler_registry,
      std::unique_ptr<syncer::DeviceInfo> device_info,
      base::OnceCallback<void(const std::string&)> on_closed,
      mojo::PendingReceiver<sharing::mojom::SharingWebRtcConnectionDelegate>
          delegate,
      mojo::PendingRemote<sharing::mojom::SharingWebRtcConnection> connection,
      mojo::PendingReceiver<network::mojom::P2PTrustedSocketManagerClient>
          socket_manager_client,
      mojo::PendingRemote<network::mojom::P2PTrustedSocketManager>
          socket_manager);
  SharingWebRtcConnectionHost(const SharingWebRtcConnectionHost&) = delete;
  SharingWebRtcConnectionHost& operator=(const SharingWebRtcConnectionHost&) =
      delete;
  ~SharingWebRtcConnectionHost() override;

  // sharing::mojom::SharingWebRtcConnectionDelegate:
  void OnMessageReceived(const std::vector<uint8_t>& message) override;

  void SendMessage(
      chrome_browser_sharing::WebRtcMessage message,
      sharing::mojom::SharingWebRtcConnection::SendMessageCallback callback);
  void OnOfferReceived(const std::string& offer,
                       base::OnceCallback<void(const std::string&)> callback);
  void OnIceCandidatesReceived(
      std::vector<sharing::mojom::IceCandidatePtr> ice_candidates);

 private:
  void OnMessageHandled(
      const std::string& original_message_id,
      chrome_browser_sharing::MessageType original_message_type,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response);
  void OnAckSent(sharing::mojom::SendMessageResult result);
  void OnConnectionClosing();
  void OnConnectionClosed();
  void OnConnectionTimeout();

  // network::mojom::P2PTrustedSocketManagerClient:
  void InvalidSocketPortRangeRequested() override;
  void DumpPacket(const std::vector<uint8_t>& packet_header,
                  uint64_t packet_length,
                  bool incoming) override;

  std::unique_ptr<WebRtcSignallingHostFCM> signalling_host_;
  // Owned by the SharingService KeyedService and must outlive |this|.
  SharingHandlerRegistry* handler_registry_;
  std::unique_ptr<syncer::DeviceInfo> device_info_;
  base::OnceCallback<void(const std::string&)> on_closed_;

  mojo::Receiver<sharing::mojom::SharingWebRtcConnectionDelegate> delegate_;
  mojo::Remote<sharing::mojom::SharingWebRtcConnection> connection_;
  mojo::Receiver<network::mojom::P2PTrustedSocketManagerClient>
      socket_manager_client_;
  mojo::Remote<network::mojom::P2PTrustedSocketManager> socket_manager_;

  sharing::WebRtcTimeoutState timeout_state_;
  // Closes the connection if it times out so we don't get stuck trying to
  // connect to a remote device.
  base::DelayTimer timeout_timer_;

  base::WeakPtrFactory<SharingWebRtcConnectionHost> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_WEBRTC_SHARING_WEBRTC_CONNECTION_HOST_H_
