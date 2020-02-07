// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_SHARING_WEBRTC_CONNECTION_H_
#define CHROME_SERVICES_SHARING_WEBRTC_SHARING_WEBRTC_CONNECTION_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/services/sharing/public/mojom/webrtc.mojom.h"
#include "chrome/services/sharing/webrtc/sharing_webrtc_timing_recorder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "third_party/webrtc/api/data_channel_interface.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

namespace webrtc {
class SessionDescriptionInterface;
}  // namespace webrtc

namespace sharing {

class IpcPacketSocketFactory;

// Manages a WebRTC PeerConnection. Signalling is handled via the passed sender
// and receiver. All network communication is handled by the network service via
// the passed P2PSocketManager and MdnsResponder pipes. All methods of this
// class are called on the same thread and the PeerConnectionFactory is setup to
// use the same thread as well.
class SharingWebRtcConnection : public mojom::SignallingReceiver,
                                public mojom::SharingWebRtcConnection,
                                public webrtc::PeerConnectionObserver,
                                public webrtc::DataChannelObserver {
 public:
  SharingWebRtcConnection(
      webrtc::PeerConnectionFactoryInterface* connection_factory,
      const std::vector<mojom::IceServerPtr>& ice_servers,
      mojo::PendingRemote<mojom::SignallingSender> signalling_sender,
      mojo::PendingReceiver<mojom::SignallingReceiver> signalling_receiver,
      mojo::PendingRemote<mojom::SharingWebRtcConnectionDelegate> delegate,
      mojo::PendingReceiver<mojom::SharingWebRtcConnection> connection,
      mojo::PendingRemote<network::mojom::P2PSocketManager> socket_manager,
      mojo::PendingRemote<network::mojom::MdnsResponder> mdns_responder,
      base::OnceCallback<void(SharingWebRtcConnection*)> on_disconnect);
  SharingWebRtcConnection(const SharingWebRtcConnection&) = delete;
  SharingWebRtcConnection& operator=(const SharingWebRtcConnection&) = delete;
  ~SharingWebRtcConnection() override;

  // mojom::SignallingReceiver:
  void OnOfferReceived(const std::string& offer,
                       OnOfferReceivedCallback callback) override;
  void OnIceCandidatesReceived(
      std::vector<mojom::IceCandidatePtr> ice_candidates) override;

  // mojom::SharingWebRtcConnection:
  void SendMessage(const std::vector<uint8_t>& message,
                   SendMessageCallback callback) override;

  // webrtc::PeerConnectionObserver:
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
  void OnRenegotiationNeeded() override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;

  // webrtc::DataChannelObserver:
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;

  // Number of bytes available to be buffered. Calling SendMessage with a
  // |message| larger than this will fail.
  size_t AvailableBufferSize() const;

 private:
  struct PendingMessage {
    PendingMessage(webrtc::DataBuffer buffer, SendMessageCallback callback);
    PendingMessage(PendingMessage&& other);
    PendingMessage& operator=(PendingMessage&& other);
    ~PendingMessage();

    webrtc::DataBuffer buffer;
    SendMessageCallback callback;
  };

  void CreateDataChannel();

  void CreateAnswer(OnOfferReceivedCallback callback, webrtc::RTCError error);
  void SetLocalAnswer(
      OnOfferReceivedCallback callback,
      std::unique_ptr<webrtc::SessionDescriptionInterface> description,
      webrtc::RTCError error);
  void OnAnswerCreated(OnOfferReceivedCallback callback,
                       std::string sdp,
                       webrtc::RTCError error);

  void CreateOffer();
  void SetLocalOffer(
      std::unique_ptr<webrtc::SessionDescriptionInterface> description,
      webrtc::RTCError error);
  void OnOfferCreated(const std::string& sdp, webrtc::RTCError error);
  void OnAnswerReceived(const std::string& answer);
  void OnRemoteDescriptionSet(webrtc::RTCError error);

  void AddIceCandidates(std::vector<mojom::IceCandidatePtr> ice_candidates);

  void OnNetworkConnectionLost();

  void CloseConnection();

  void MaybeSendQueuedMessages();

  void HandlePendingIceCandidates();

  void OnStatsReceived(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report);

  mojo::Receiver<mojom::SignallingReceiver> signalling_receiver_;
  mojo::Remote<mojom::SignallingSender> signalling_sender_;
  mojo::Receiver<mojom::SharingWebRtcConnection> connection_;
  mojo::Remote<mojom::SharingWebRtcConnectionDelegate> delegate_;
  mojo::Remote<network::mojom::P2PSocketManager> p2p_socket_manager_;
  mojo::Remote<network::mojom::MdnsResponder> mdns_responder_;

  std::unique_ptr<IpcPacketSocketFactory> socket_factory_;
  scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  scoped_refptr<webrtc::DataChannelInterface> channel_;

  // Temporary storage for messages and ICE candidates while the connection is
  // being initiated.
  base::queue<PendingMessage> queued_messages_;
  size_t queued_messages_total_size_ = 0;
  std::vector<mojom::IceCandidatePtr> remote_ice_candidates_;
  std::vector<mojom::IceCandidatePtr> local_ice_candidates_;

  base::OnceCallback<void(SharingWebRtcConnection*)> on_disconnect_;
  SharingWebRtcTimingRecorder timing_recorder_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SharingWebRtcConnection> weak_ptr_factory_{this};
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_SHARING_WEBRTC_CONNECTION_H_
