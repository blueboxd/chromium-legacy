// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/sharing_webrtc_connection.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/services/sharing/public/cpp/sharing_webrtc_metrics.h"
#include "chrome/services/sharing/webrtc/ipc_network_manager.h"
#include "chrome/services/sharing/webrtc/ipc_packet_socket_factory.h"
#include "chrome/services/sharing/webrtc/mdns_responder_adapter.h"
#include "chrome/services/sharing/webrtc/p2p_port_allocator.h"
#include "third_party/webrtc/api/jsep.h"
#include "third_party/webrtc/api/stats/rtc_stats_collector_callback.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"

namespace {

constexpr char kChannelName[] = "chrome-sharing";
// This needs to be less or equal to the WebRTC DataChannel buffer size.
constexpr size_t kMaxQueuedSendDataBytes = 16 * 1024 * 1024;
// Individual message size limit. Needs to be less or equal to the
// SctpSendBufferSize.
constexpr size_t kMaxMessageSize = 256 * 1024;

// A webrtc::SetSessionDescriptionObserver implementation used to receive the
// results of setting local and remote descriptions of the PeerConnection.
class SetSessionDescriptionObserverWrapper
    : public webrtc::SetSessionDescriptionObserver {
 public:
  typedef base::OnceCallback<void(webrtc::RTCError error)> ResultCallback;

  SetSessionDescriptionObserverWrapper(
      const SetSessionDescriptionObserverWrapper&) = delete;
  SetSessionDescriptionObserverWrapper& operator=(
      const SetSessionDescriptionObserverWrapper&) = delete;

  static SetSessionDescriptionObserverWrapper* Create(
      ResultCallback result_callback) {
    return new rtc::RefCountedObject<SetSessionDescriptionObserverWrapper>(
        std::move(result_callback));
  }
  void OnSuccess() override {
    std::move(result_callback_).Run(webrtc::RTCError::OK());
  }
  void OnFailure(webrtc::RTCError error) override {
    std::move(result_callback_).Run(std::move(error));
  }

 protected:
  explicit SetSessionDescriptionObserverWrapper(ResultCallback result_callback)
      : result_callback_(std::move(result_callback)) {}
  ~SetSessionDescriptionObserverWrapper() override = default;

 private:
  ResultCallback result_callback_;
};

// A webrtc::CreateSessionDescriptionObserver implementation used to receive the
// results of creating descriptions for this end of the PeerConnection.
class CreateSessionDescriptionObserverWrapper
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  typedef base::OnceCallback<void(
      std::unique_ptr<webrtc::SessionDescriptionInterface> description,
      webrtc::RTCError error)>
      ResultCallback;

  CreateSessionDescriptionObserverWrapper(
      const CreateSessionDescriptionObserverWrapper&) = delete;
  CreateSessionDescriptionObserverWrapper& operator=(
      const CreateSessionDescriptionObserverWrapper&) = delete;

  static CreateSessionDescriptionObserverWrapper* Create(
      ResultCallback result_callback) {
    return new rtc::RefCountedObject<CreateSessionDescriptionObserverWrapper>(
        std::move(result_callback));
  }
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    std::move(result_callback_)
        .Run(base::WrapUnique(desc), webrtc::RTCError::OK());
  }
  void OnFailure(webrtc::RTCError error) override {
    std::move(result_callback_).Run(nullptr, std::move(error));
  }

 protected:
  explicit CreateSessionDescriptionObserverWrapper(
      ResultCallback result_callback)
      : result_callback_(std::move(result_callback)) {}
  ~CreateSessionDescriptionObserverWrapper() override = default;

 private:
  ResultCallback result_callback_;
};

class ProxyAsyncResolverFactory final : public webrtc::AsyncResolverFactory {
 public:
  explicit ProxyAsyncResolverFactory(
      sharing::IpcPacketSocketFactory* socket_factory)
      : socket_factory_(socket_factory) {
    DCHECK(socket_factory_);
  }

  rtc::AsyncResolverInterface* Create() override {
    return socket_factory_->CreateAsyncResolver();
  }

 private:
  sharing::IpcPacketSocketFactory* socket_factory_;
};

class RTCStatsCollectorCallback : public webrtc::RTCStatsCollectorCallback {
 public:
  typedef base::OnceCallback<void(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report)>
      ResultCallback;

  RTCStatsCollectorCallback(const RTCStatsCollectorCallback&) = delete;
  RTCStatsCollectorCallback& operator=(const RTCStatsCollectorCallback&) =
      delete;

  static RTCStatsCollectorCallback* Create(ResultCallback result_callback) {
    return new rtc::RefCountedObject<RTCStatsCollectorCallback>(
        std::move(result_callback));
  }

  void OnStatsDelivered(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override {
    std::move(result_callback_).Run(report);
  }

 protected:
  explicit RTCStatsCollectorCallback(ResultCallback result_callback)
      : result_callback_(std::move(result_callback)) {}
  ~RTCStatsCollectorCallback() override = default;

 private:
  ResultCallback result_callback_;
};

}  // namespace

namespace sharing {

SharingWebRtcConnection::SharingWebRtcConnection(
    webrtc::PeerConnectionFactoryInterface* connection_factory,
    const std::vector<mojom::IceServerPtr>& ice_servers,
    mojo::PendingRemote<mojom::SignallingSender> signalling_sender,
    mojo::PendingReceiver<mojom::SignallingReceiver> signalling_receiver,
    mojo::PendingRemote<mojom::SharingWebRtcConnectionDelegate> delegate,
    mojo::PendingReceiver<mojom::SharingWebRtcConnection> connection,
    mojo::PendingRemote<network::mojom::P2PSocketManager> socket_manager,
    mojo::PendingRemote<network::mojom::MdnsResponder> mdns_responder,
    base::OnceCallback<void(SharingWebRtcConnection*)> on_disconnect)
    : signalling_receiver_(this, std::move(signalling_receiver)),
      signalling_sender_(std::move(signalling_sender)),
      connection_(this, std::move(connection)),
      delegate_(std::move(delegate)),
      p2p_socket_manager_(std::move(socket_manager)),
      mdns_responder_(std::move(mdns_responder)),
      on_disconnect_(std::move(on_disconnect)) {
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  for (const auto& ice_server : ice_servers) {
    webrtc::PeerConnectionInterface::IceServer ice_turn_server;
    for (const auto& url : ice_server->urls)
      ice_turn_server.urls.push_back(url.spec());
    if (ice_server->username)
      ice_turn_server.username = *ice_server->username;
    if (ice_server->credential)
      ice_turn_server.password = *ice_server->credential;
    rtc_config.servers.push_back(ice_turn_server);
  }

  signalling_sender_.set_disconnect_handler(
      base::BindOnce(&SharingWebRtcConnection::CloseConnection,
                     weak_ptr_factory_.GetWeakPtr()));
  delegate_.set_disconnect_handler(
      base::BindOnce(&SharingWebRtcConnection::CloseConnection,
                     weak_ptr_factory_.GetWeakPtr()));

  p2p_socket_manager_.set_disconnect_handler(
      base::BindOnce(&SharingWebRtcConnection::OnNetworkConnectionLost,
                     weak_ptr_factory_.GetWeakPtr()));
  mdns_responder_.set_disconnect_handler(
      base::BindOnce(&SharingWebRtcConnection::OnNetworkConnectionLost,
                     weak_ptr_factory_.GetWeakPtr()));

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("sharing_webrtc_connection", R"(
        semantics {
          sender: "Chrome Sharing via WebRTC"
          description:
            "Chrome Sharing allows users to send data securely between their "
            "devices. WebRTC allows Chrome to establish a secure session with "
            "another Chrome instance running on a different device and to "
            "transmit and receive data that users want to share across their "
            "devices. The source data depends on the Sharing feature used, "
            "e.g. selected text for SharedClipboard or phone numbers for Click "
            "to Call."
          trigger:
            "User uses the Sharing feature and selects one of their devices to "
            "send the data to."
          data:
            "Text and media encrypted via AES-128-GCM. Protocol-level messages "
            "for the various subprotocols employed by WebRTC (including ICE, "
            "DTLS, RTCP, etc.) are encrypted via DTLS-SRTP. Note that ICE "
            "connectivity checks may leak the user's IP address(es), subject "
            "to the restrictions/guidance in "
            "https://datatracker.ietf.org/doc/draft-ietf-rtcweb-ip-handling."
          destination: OTHER
          destination_other:
            "A remote Chrome instance that receives this data in a sandboxed "
            "process."
        }
        policy {
          cookies_allowed: NO
          setting: "This feature can be disabled by signing out of Chrome."
          chrome_policy {
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        }
    )");

  socket_factory_ = std::make_unique<IpcPacketSocketFactory>(
      p2p_socket_manager_.get(), traffic_annotation);

  auto network_manager = std::make_unique<IpcNetworkManager>(
      p2p_socket_manager_.get(),
      std::make_unique<MdnsResponderAdapter>(mdns_responder_.get()));

  webrtc::PeerConnectionDependencies dependencies(this);
  P2PPortAllocator::Config port_config;
  port_config.enable_multiple_routes = true;
  port_config.enable_nonproxied_udp = true;
  dependencies.allocator = std::make_unique<P2PPortAllocator>(
      std::move(network_manager), socket_factory_.get(), port_config);
  dependencies.async_resolver_factory =
      std::make_unique<ProxyAsyncResolverFactory>(socket_factory_.get());

  peer_connection_ = connection_factory->CreatePeerConnection(
      rtc_config, std::move(dependencies));
  CHECK(peer_connection_);
}

SharingWebRtcConnection::~SharingWebRtcConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseConnection();
  peer_connection_->Close();
  timing_recorder_.LogEvent(WebRtcTimingEvent::kClosed);
}

void SharingWebRtcConnection::OnOfferReceived(
    const std::string& offer,
    OnOfferReceivedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only the receiver receives an offer from the sender and it's the first
  // thing it does.
  timing_recorder_.set_is_sender(false);

  timing_recorder_.LogEvent(WebRtcTimingEvent::kOfferReceived);

  std::unique_ptr<webrtc::SessionDescriptionInterface> description(
      webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, offer,
                                       nullptr));
  if (!description) {
    LogWebRtcConnectionErrorReason(
        WebRtcConnectionErrorReason::kInvalidRemoteOffer);
    CloseConnection();
    return;
  }

  peer_connection_->SetRemoteDescription(
      SetSessionDescriptionObserverWrapper::Create(
          base::BindOnce(&SharingWebRtcConnection::CreateAnswer,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))),
      description.release());
}

void SharingWebRtcConnection::OnIceCandidatesReceived(
    std::vector<mojom::IceCandidatePtr> ice_candidates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ice_candidates.empty())
    return;

  timing_recorder_.LogEvent(WebRtcTimingEvent::kIceCandidateReceived);

  remote_ice_candidates_.insert(remote_ice_candidates_.end(),
                                std::make_move_iterator(ice_candidates.begin()),
                                std::make_move_iterator(ice_candidates.end()));
  HandlePendingIceCandidates();
}

void SharingWebRtcConnection::SendMessage(const std::vector<uint8_t>& message,
                                          SendMessageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We know that we're the sender if there is no data channel yet because the
  // receiver only sends ack messages after receiving via a data channel.
  if (!channel_)
    timing_recorder_.set_is_sender(true);

  timing_recorder_.LogEvent(WebRtcTimingEvent::kQueuingMessage);

  if (message.empty()) {
    LogWebRtcSendMessageResult(WebRtcSendMessageResult::kEmptyMessage);
    std::move(callback).Run(mojom::SendMessageResult::kError);
    return;
  }

  // TODO(crbug.com/1039280): Split / merge logic for messages > 256k.
  if (message.size() > kMaxMessageSize) {
    LogWebRtcSendMessageResult(WebRtcSendMessageResult::kPayloadTooLarge);
    std::move(callback).Run(mojom::SendMessageResult::kError);
    return;
  }

  if (message.size() > AvailableBufferSize()) {
    LogWebRtcSendMessageResult(WebRtcSendMessageResult::kBufferExceeded);
    std::move(callback).Run(mojom::SendMessageResult::kError);
    return;
  }

  if (!channel_)
    CreateDataChannel();

  if (!channel_) {
    LogWebRtcSendMessageResult(WebRtcSendMessageResult::kDataChannelNotReady);
    CloseConnection();
    std::move(callback).Run(mojom::SendMessageResult::kError);
    return;
  }

  webrtc::DataBuffer buffer(
      rtc::CopyOnWriteBuffer(message.data(), message.size()), /*binary=*/true);

  // Queue this message until the DataChannel is ready and all queued messages
  // have been sent.
  if (channel_->state() ==
          webrtc::DataChannelInterface::DataState::kConnecting ||
      !queued_messages_.empty()) {
    queued_messages_total_size_ += buffer.size();
    queued_messages_.emplace(std::move(buffer), std::move(callback));
    return;
  }

  if (channel_->state() != webrtc::DataChannelInterface::DataState::kOpen) {
    LogWebRtcSendMessageResult(WebRtcSendMessageResult::kDataChannelNotReady);
    std::move(callback).Run(mojom::SendMessageResult::kError);
    return;
  }

  timing_recorder_.LogEvent(WebRtcTimingEvent::kSendingMessage);

  if (!channel_->Send(std::move(buffer))) {
    LogWebRtcSendMessageResult(WebRtcSendMessageResult::kInternalError);
    CloseConnection();
    std::move(callback).Run(mojom::SendMessageResult::kError);
    return;
  }

  std::move(callback).Run(mojom::SendMessageResult::kSuccess);
}

void SharingWebRtcConnection::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  if (new_state != webrtc::PeerConnectionInterface::SignalingState::kStable)
    return;

  timing_recorder_.LogEvent(WebRtcTimingEvent::kSignalingStable);
  HandlePendingIceCandidates();
}

void SharingWebRtcConnection::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  channel_ = data_channel;
  channel_->RegisterObserver(this);
}

void SharingWebRtcConnection::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  if (new_state !=
      webrtc::PeerConnectionInterface::PeerConnectionState::kConnected)
    return;

  peer_connection_->GetStats(RTCStatsCollectorCallback::Create(
      base::BindOnce(&SharingWebRtcConnection::OnStatsReceived,
                     weak_ptr_factory_.GetWeakPtr())));
}

void SharingWebRtcConnection::OnRenegotiationNeeded() {
  CreateOffer();
}

void SharingWebRtcConnection::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {}

void SharingWebRtcConnection::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string candidate_string;
  if (!candidate->ToString(&candidate_string)) {
    LOG(ERROR) << "Failed to serialize IceCandidate";
    return;
  }

  local_ice_candidates_.push_back(mojom::IceCandidate::New(
      candidate_string, candidate->sdp_mid(), candidate->sdp_mline_index()));

  HandlePendingIceCandidates();
}

void SharingWebRtcConnection::OnStateChange() {
  switch (channel_->state()) {
    case webrtc::DataChannelInterface::DataState::kOpen:
      timing_recorder_.LogEvent(WebRtcTimingEvent::kDataChannelOpen);
      // Post a task here as we might end up sending a new message which is not
      // allowed from observer callbacks.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&SharingWebRtcConnection::MaybeSendQueuedMessages,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    case webrtc::DataChannelInterface::DataState::kClosed:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&SharingWebRtcConnection::CloseConnection,
                                    weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      break;
  }
}

void SharingWebRtcConnection::OnMessage(const webrtc::DataBuffer& buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timing_recorder_.LogEvent(WebRtcTimingEvent::kMessageReceived);
  std::vector<uint8_t> data(buffer.size());
  memcpy(data.data(), buffer.data.cdata(), buffer.size());
  delegate_->OnMessageReceived(data);
}

size_t SharingWebRtcConnection::AvailableBufferSize() const {
  size_t buffered = channel_ ? channel_->buffered_amount() : 0;
  size_t total_used = buffered + queued_messages_total_size_;
  if (total_used > kMaxQueuedSendDataBytes)
    return 0;
  return kMaxQueuedSendDataBytes - total_used;
}

void SharingWebRtcConnection::CreateDataChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  webrtc::DataChannelInit data_channel_init;
  data_channel_init.reliable = true;
  channel_ =
      peer_connection_->CreateDataChannel(kChannelName, &data_channel_init);
  if (channel_)
    channel_->RegisterObserver(this);
}

void SharingWebRtcConnection::CreateAnswer(OnOfferReceivedCallback callback,
                                           webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!error.ok()) {
    LogWebRtcConnectionErrorReason(
        WebRtcConnectionErrorReason::kInvalidRemoteOffer);
    CloseConnection();
    return;
  }

  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  peer_connection_->CreateAnswer(
      CreateSessionDescriptionObserverWrapper::Create(
          base::BindOnce(&SharingWebRtcConnection::SetLocalAnswer,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))),
      options);
}

void SharingWebRtcConnection::SetLocalAnswer(
    OnOfferReceivedCallback callback,
    std::unique_ptr<webrtc::SessionDescriptionInterface> description,
    webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string sdp;
  if (!error.ok() || !description || !description->ToString(&sdp)) {
    LogWebRtcConnectionErrorReason(
        WebRtcConnectionErrorReason::kInvalidLocalAnswer);
    CloseConnection();
    return;
  }

  peer_connection_->SetLocalDescription(
      SetSessionDescriptionObserverWrapper::Create(base::BindOnce(
          &SharingWebRtcConnection::OnAnswerCreated,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), std::move(sdp))),
      description.release());
}

void SharingWebRtcConnection::OnAnswerCreated(OnOfferReceivedCallback callback,
                                              std::string sdp,
                                              webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!error.ok()) {
    LogWebRtcConnectionErrorReason(
        WebRtcConnectionErrorReason::kInvalidLocalAnswer);
    CloseConnection();
    return;
  }

  timing_recorder_.LogEvent(WebRtcTimingEvent::kAnswerCreated);

  HandlePendingIceCandidates();
  std::move(callback).Run(sdp);
}

void SharingWebRtcConnection::CreateOffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  peer_connection_->CreateOffer(
      CreateSessionDescriptionObserverWrapper::Create(
          base::BindOnce(&SharingWebRtcConnection::SetLocalOffer,
                         weak_ptr_factory_.GetWeakPtr())),
      options);
}

void SharingWebRtcConnection::SetLocalOffer(
    std::unique_ptr<webrtc::SessionDescriptionInterface> description,
    webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string sdp;
  if (!error.ok() || !description || !description->ToString(&sdp)) {
    LogWebRtcConnectionErrorReason(
        WebRtcConnectionErrorReason::kInvalidLocalOffer);
    CloseConnection();
    return;
  }

  peer_connection_->SetLocalDescription(
      SetSessionDescriptionObserverWrapper::Create(
          base::BindOnce(&SharingWebRtcConnection::OnOfferCreated,
                         weak_ptr_factory_.GetWeakPtr(), std::move(sdp))),
      description.release());
}

void SharingWebRtcConnection::OnOfferCreated(const std::string& sdp,
                                             webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!error.ok()) {
    LogWebRtcConnectionErrorReason(
        WebRtcConnectionErrorReason::kInvalidLocalOffer);
    CloseConnection();
    return;
  }

  timing_recorder_.LogEvent(WebRtcTimingEvent::kOfferCreated);

  signalling_sender_->SendOffer(
      sdp, base::BindOnce(&SharingWebRtcConnection::OnAnswerReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}

void SharingWebRtcConnection::OnAnswerReceived(const std::string& answer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<webrtc::SessionDescriptionInterface> description(
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, answer,
                                       nullptr));
  if (!description) {
    LogWebRtcConnectionErrorReason(
        WebRtcConnectionErrorReason::kInvalidRemoteAnswer);
    CloseConnection();
    return;
  }

  timing_recorder_.LogEvent(WebRtcTimingEvent::kAnswerReceived);

  peer_connection_->SetRemoteDescription(
      SetSessionDescriptionObserverWrapper::Create(
          base::BindOnce(&SharingWebRtcConnection::OnRemoteDescriptionSet,
                         weak_ptr_factory_.GetWeakPtr())),
      description.release());
}

void SharingWebRtcConnection::OnRemoteDescriptionSet(webrtc::RTCError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!error.ok()) {
    LogWebRtcConnectionErrorReason(
        WebRtcConnectionErrorReason::kInvalidRemoteAnswer);
    CloseConnection();
    return;
  }

  HandlePendingIceCandidates();
}

void SharingWebRtcConnection::AddIceCandidates(
    std::vector<mojom::IceCandidatePtr> ice_candidates) {
  for (const auto& ice_candidate : ice_candidates) {
    std::unique_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(
            ice_candidate->sdp_mid, ice_candidate->sdp_mline_index,
            ice_candidate->candidate, /*error=*/nullptr));

    if (candidate) {
      peer_connection_->AddIceCandidate(
          std::move(candidate),
          [](webrtc::RTCError error) { LogWebRtcAddIceCandidate(error.ok()); });
    } else {
      LogWebRtcAddIceCandidate(false);
    }
  }
}

void SharingWebRtcConnection::OnNetworkConnectionLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Force close the DataChannel if we lost network access.
  if (channel_) {
    channel_->UnregisterObserver();
    channel_ = nullptr;
  }

  CloseConnection();
}

void SharingWebRtcConnection::CloseConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timing_recorder_.LogEvent(WebRtcTimingEvent::kClosing);

  // Call all queued callbacks.
  while (!queued_messages_.empty()) {
    LogWebRtcSendMessageResult(WebRtcSendMessageResult::kConnectionClosed);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(queued_messages_.front().callback),
                                  mojom::SendMessageResult::kError));
    queued_messages_.pop();
  }

  signalling_sender_.reset();
  delegate_.reset();

  // Close DataChannel if necessary.
  if (channel_) {
    switch (channel_->state()) {
      case webrtc::DataChannelInterface::DataState::kClosing:
        // DataChannel is still going through the close procedure and will call
        // OnStateChange when done.
        return;
      case webrtc::DataChannelInterface::DataState::kConnecting:
      case webrtc::DataChannelInterface::DataState::kClosed:
        channel_->UnregisterObserver();
        channel_ = nullptr;
        break;
      case webrtc::DataChannelInterface::DataState::kOpen:
        // Start the closing procedure of the DataChannel.
        channel_->Close();
        return;
    }
  }

  // DataChannel must be closed by this point.
  DCHECK(!channel_);

  if (on_disconnect_)
    std::move(on_disconnect_).Run(this);
  // Note: |this| might be destroyed here.
}

void SharingWebRtcConnection::MaybeSendQueuedMessages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (channel_->state() != webrtc::DataChannelInterface::DataState::kOpen)
    return;

  timing_recorder_.LogEvent(WebRtcTimingEvent::kSendingMessage);

  // Send all queued messages. All of them should fit into the DataChannel
  // buffer as we checked the total size before accepting new messages.
  while (!queued_messages_.empty()) {
    PendingMessage pending_message(std::move(queued_messages_.front()));
    queued_messages_.pop();

    bool success = channel_->Send(std::move(pending_message.buffer));
    LogWebRtcSendMessageResult(success
                                   ? WebRtcSendMessageResult::kSuccess
                                   : WebRtcSendMessageResult::kInternalError);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(pending_message.callback),
                                  success ? mojom::SendMessageResult::kSuccess
                                          : mojom::SendMessageResult::kError));

    if (!success) {
      CloseConnection();
      return;
    }
  }
  queued_messages_total_size_ = 0;
}

void SharingWebRtcConnection::HandlePendingIceCandidates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Store received ICE candidates until the signalling state is stable and
  // there is no offer / answer exchange in progress anymore.
  if (!peer_connection_->local_description() ||
      peer_connection_->signaling_state() !=
          webrtc::PeerConnectionInterface::SignalingState::kStable) {
    return;
  }

  if (!remote_ice_candidates_.empty()) {
    AddIceCandidates(std::move(remote_ice_candidates_));
    remote_ice_candidates_.clear();
  }

  if (!local_ice_candidates_.empty()) {
    signalling_sender_->SendIceCandidates(std::move(local_ice_candidates_));
    local_ice_candidates_.clear();
  }
}

void SharingWebRtcConnection::OnStatsReceived(
    const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
  auto transport_stats_list =
      report->GetStatsOfType<webrtc::RTCTransportStats>();
  if (transport_stats_list.size() != 1) {
    LogWebRtcConnectionType(WebRtcConnectionType::kInvalid);
    return;
  }

  const webrtc::RTCStats* selected_candidate_pair =
      report->Get(*(transport_stats_list[0]->selected_candidate_pair_id));
  if (!selected_candidate_pair) {
    LogWebRtcConnectionType(WebRtcConnectionType::kInvalid);
    return;
  }

  std::string local_candidate_id =
      *(selected_candidate_pair->cast_to<webrtc::RTCIceCandidatePairStats>()
            .local_candidate_id);
  const webrtc::RTCStats* local_candidate = report->Get(local_candidate_id);
  if (!local_candidate) {
    LogWebRtcConnectionType(WebRtcConnectionType::kInvalid);
    return;
  }

  std::string candidate_type =
      *(local_candidate->cast_to<webrtc::RTCLocalIceCandidateStats>()
            .candidate_type);
  LogWebRtcConnectionType(StringToWebRtcConnectionType(candidate_type));
}

SharingWebRtcConnection::PendingMessage::PendingMessage(
    webrtc::DataBuffer buffer,
    SendMessageCallback callback)
    : buffer(std::move(buffer)), callback(std::move(callback)) {}

SharingWebRtcConnection::PendingMessage::PendingMessage(
    PendingMessage&& other) = default;

SharingWebRtcConnection::PendingMessage&
SharingWebRtcConnection::PendingMessage::operator=(PendingMessage&& other) =
    default;

SharingWebRtcConnection::PendingMessage::~PendingMessage() = default;

}  // namespace sharing
